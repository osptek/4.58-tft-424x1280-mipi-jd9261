/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "soc/soc_caps.h"

#if SOC_MIPI_DSI_SUPPORTED
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_vendor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_lcd_jd9261.h"
#include "jd9261_interface.h"

typedef struct {
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    uint8_t madctl_val; // save current value of LCD_CMD_MADCTL register
    uint8_t colmod_val; // save surrent value of LCD_CMD_COLMOD register
    const jd9261_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    struct {
        unsigned int reset_level: 1;
    } flags;
    // To save the original functions of MIPI DPI panel
    esp_err_t (*del)(esp_lcd_panel_t *panel);
    esp_err_t (*init)(esp_lcd_panel_t *panel);
} jd9261_panel_t;

static const char *TAG = "jd9261";

static esp_err_t panel_jd9261_del(esp_lcd_panel_t *panel);
static esp_err_t panel_jd9261_init(esp_lcd_panel_t *panel);
static esp_err_t panel_jd9261_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_jd9261_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_jd9261_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_jd9261_disp_on_off(esp_lcd_panel_t *panel, bool on_off);

esp_err_t esp_lcd_new_panel_jd9261_mipi(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                         esp_lcd_panel_handle_t *ret_panel)
{
    ESP_RETURN_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");
    jd9261_vendor_config_t *vendor_config = (jd9261_vendor_config_t *)panel_dev_config->vendor_config;
    ESP_RETURN_ON_FALSE(vendor_config && vendor_config->mipi_config.dpi_config && vendor_config->mipi_config.dsi_bus, ESP_ERR_INVALID_ARG, TAG,
                        "invalid vendor config");

    esp_err_t ret = ESP_OK;
    jd9261_panel_t *jd9261 = (jd9261_panel_t *)calloc(1, sizeof(jd9261_panel_t));
    ESP_RETURN_ON_FALSE(jd9261, ESP_ERR_NO_MEM, TAG, "no mem for jd9261 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    switch (panel_dev_config->color_space) {
    case LCD_RGB_ELEMENT_ORDER_RGB:
        jd9261->madctl_val = 0;
        break;
    case LCD_RGB_ELEMENT_ORDER_BGR:
        jd9261->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color space");
        break;
    }

    switch (panel_dev_config->bits_per_pixel) {
    case 16: // RGB565
        jd9261->colmod_val = 0x55;
        break;
    case 18: // RGB666
        jd9261->colmod_val = 0x66;
        break;
    case 24: // RGB888
        jd9261->colmod_val = 0x77;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    uint8_t ID[3];
    ESP_GOTO_ON_ERROR(esp_lcd_panel_io_rx_param(io, 0x04, ID, 3), err, TAG, "read ID failed");
    ESP_LOGI(TAG, "LCD ID: %02X %02X %02X", ID[0], ID[1], ID[2]);

    jd9261->io = io;
    jd9261->init_cmds = vendor_config->init_cmds;
    jd9261->init_cmds_size = vendor_config->init_cmds_size;
    jd9261->reset_gpio_num = panel_dev_config->reset_gpio_num;
    jd9261->flags.reset_level = panel_dev_config->flags.reset_active_high;

    // Create MIPI DPI panel
    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_dpi(vendor_config->mipi_config.dsi_bus, vendor_config->mipi_config.dpi_config, &panel_handle), err, TAG,
                      "create MIPI DPI panel failed");
    ESP_LOGD(TAG, "new MIPI DPI panel @%p", panel_handle);

    // Save the original functions of MIPI DPI panel
    jd9261->del = panel_handle->del;
    jd9261->init = panel_handle->init;
    // Overwrite the functions of MIPI DPI panel
    panel_handle->del = panel_jd9261_del;
    panel_handle->init = panel_jd9261_init;
    panel_handle->reset = panel_jd9261_reset;
    panel_handle->mirror = panel_jd9261_mirror;
    panel_handle->invert_color = panel_jd9261_invert_color;
    panel_handle->disp_on_off = panel_jd9261_disp_on_off;
    panel_handle->user_data = jd9261;
    *ret_panel = panel_handle;
    ESP_LOGD(TAG, "new jd9261 panel @%p", jd9261);

    return ESP_OK;

err:
    if (jd9261) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(jd9261);
    }
    return ret;
}

static const jd9261_lcd_init_cmd_t vendor_specific_init_default[] = {
// {cmd, { data }, data_size, delay_ms}
{0xDF, (uint8_t []){0x90, 0x62, 0xF2}, 3, 1}, 
{0xDE, (uint8_t []){0x00}, 1, 1}, 
{0xCC, (uint8_t []){0x31}, 1, 0}, 
{0xB2, (uint8_t []){0x01, 0x23, 0x61, 0x88, 0xE4}, 5, 1}, 
{0xBB, (uint8_t []){0x0C, 0x44, 0x6E, 0x32, 0x20, 0x20}, 6, 1}, 
{0xBD, (uint8_t []){0x00, 0xBF}, 2, 1}, 
{0xBF, (uint8_t []){0x64, 0x32, 0x33, 0xC3}, 4, 1}, 
{0xC0, (uint8_t []){0x00, 0x95, 0x00, 0x95}, 4, 1},
{0xC1, (uint8_t []){0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55}, 10, 1},
{0xC3, (uint8_t []){0x0A, 0x02, 0x00, 0x00, 0x08, 0x4D, 0xFF, 0xFF, 0xFF, 0xFF, 
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
                    0x00, 0x10, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                    0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x0E, 0x00, 0x0E}, 49, 1},
{0xC4, (uint8_t []){0x02, 0x06}, 2, 1},
{0xC6, (uint8_t []){0x01, 0x04, 0x00, 0x78, 0x00, 0x14}, 6, 1},
{0xC8, (uint8_t []){0x29, 0x80, 0x6A}, 3, 1}, 
{0xCB, (uint8_t []){0x6E, 0x60, 0x55, 0x45, 0x37, 0x32, 0x23, 0x28, 0x12, 0x2D, 
                    0x2D, 0x2D, 0x4A, 0x37, 0x3E, 0x32, 0x30, 0x25, 0x15, 0x0B, 
                    0x00, 0x6E, 0x60, 0x55, 0x45, 0x37, 0x32, 0x23, 0x28, 0x12, 
                    0x2D, 0x2D, 0x2D, 0x4A, 0x37, 0x3E, 0x32, 0x30, 0x25, 0x15, 
                    0x0B, 0x00}, 42, 1},
{0xCD, (uint8_t []){0x10, 0x00, 0x10, 0x00}, 4, 1}, 
{0xCE, (uint8_t []){0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                    0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0x55, 
                    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 
                    0x55, 0x55, 0x55, 0x55, 0x55}, 35, 1},
{0xCF, (uint8_t []){0x52, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 
                    0xFF, 0x00, 0x00}, 13, 1},
{0xD0, (uint8_t []){0x00, 0x1F, 0x1F, 0x1F, 0x80, 0x8A, 0x88, 0x86, 0x84, 0x9E, 
                    0x9F, 0x90, 0x9F, 0x98, 0x97, 0x1F, 0x1F, 0x1F}, 18, 1},
{0xD1, (uint8_t []){0x00, 0x1F, 0x1F, 0x1F, 0x81, 0x8B, 0x89, 0x87, 0x85, 0x9E, 
                    0x9F, 0x91, 0x9F, 0x98, 0x97, 0x1F, 0x1F, 0x1F}, 18, 1},
{0xD2, (uint8_t []){0x00, 0x1F, 0x1F, 0x1F, 0x91, 0x85, 0x87, 0x89, 0x8B, 0x9F, 
                    0x9E, 0x81, 0x9F, 0x98, 0x97, 0x1F, 0x1F, 0x1F}, 18, 1},
{0xD3, (uint8_t []){0x00, 0x1F, 0x1F, 0x1F, 0x90, 0x84, 0x86, 0x88, 0x8A, 0x9F, 
                    0x9E, 0x80, 0x9F, 0x98, 0x97, 0x1F, 0x1F, 0x1F}, 18, 1},
{0xD4, (uint8_t []){0x00, 0x20, 0x24, 0x01, 0x00, 0x02, 0x20, 0x01, 0x00, 0x00, 
                    0x00, 0x04, 0x08, 0x81, 0x0A, 0x2B, 0x04, 0x00, 0x01, 0x01, 
                    0x11, 0x11, 0x04, 0x08, 0x11, 0x60, 0x03, 0x05, 0x01, 0x00, 
                    0x04, 0x00, 0x0A, 0x04, 0x67, 0x00, 0x0F, 0x00, 0x00, 0x00, 
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00}, 59, 1},
{0xD5, (uint8_t []){0x01, 0x10, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xA0, 
                    0x00, 0x00, 0x00, 0x07, 0x32, 0x5A, 0x00, 0x0E, 0x01, 0x00, 
                    0x01, 0x00, 0x00, 0x73, 0xA0, 0x28, 0x00, 0x00, 0x00}, 29, 1},
{0xD7, (uint8_t []){0x00, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 
                    0x38, 0x38, 0x38, 0x29, 0x47, 0x38, 0x38}, 17, 1},
{0xDE, (uint8_t []){0x01}, 1, 1},
{0xC7, (uint8_t []){0x14}, 1, 1},
{0xDE, (uint8_t []){0x02}, 1, 1}, 
{0xC2, (uint8_t []){0x02, 0xC2, 0x50, 0x00, 0x12, 0xA2, 0x61, 0x73, 0xF7}, 9, 1},
{0xBB, (uint8_t []){0x00, 0x00, 0x00, 0x00, 0x00, 0x68, 0x69}, 7, 0},
{0xC3, (uint8_t []){0x20, 0xFF, 0x00, 0xA0, 0x10, 0x82, 0x06, 0x01, 0x31, 0x53, 
                    0x64, 0x75, 0x6E, 0x82}, 14, 1},
{0xC6, (uint8_t []){0x4D}, 1, 1},
{0xD3, (uint8_t []){0x00, 0x66, 0x66, 0x14, 0x69, 0xCE, 0x02, 0x58, 0xAD, 0x14, 
                    0x69, 0xCE, 0x02, 0x58, 0xAD}, 15, 1},
{0xE6, (uint8_t []){0x10, 0x08, 0x66}, 3, 1},
{0xEC, (uint8_t []){0x03, 0x0E, 0x7F, 0x77, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                    0xF1, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 
                    0xFF, 0x01, 0x7F}, 23, 1},
{0xDE, (uint8_t []){0x03}, 1, 1},
{0xD1, (uint8_t []){0x00, 0x00}, 2, 1},
{0xDE, (uint8_t []){0x00}, 1, 1},
{0x35, (uint8_t []){0x00}, 0, 30},
{0x11, (uint8_t []){0x00}, 0, 120},
{0x29, (uint8_t []){0x00}, 0, 120},
};

static esp_err_t panel_jd9261_del(esp_lcd_panel_t *panel)
{
    jd9261_panel_t *jd9261 = (jd9261_panel_t *)panel->user_data;

    if (jd9261->reset_gpio_num >= 0) {
        gpio_reset_pin(jd9261->reset_gpio_num);
    }
    // Delete MIPI DPI panel
    jd9261->del(panel);
    ESP_LOGD(TAG, "del jd9261 panel @%p", jd9261);
    free(jd9261);

    return ESP_OK;
}

static esp_err_t panel_jd9261_init(esp_lcd_panel_t *panel)
{
    jd9261_panel_t *jd9261 = (jd9261_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = jd9261->io;
    const jd9261_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;
    bool is_command1_enable = true;
    bool is_cmd_overwritten = false;

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, JD9261_PAGE_CMD1, (uint8_t []) {
        0x00
    }, 1), TAG, "Write cmd failed");
    // Set color format
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t []) {
        jd9261->madctl_val
    }, 1), TAG, "Write cmd failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_COLMOD, (uint8_t []) {
        jd9261->colmod_val
    }, 1), TAG, "Write cmd failed");

    // vendor specific initialization, it can be different between manufacturers
    // should consult the LCD supplier for initialization sequence code
    if (jd9261->init_cmds) {
        init_cmds = jd9261->init_cmds;
        init_cmds_size = jd9261->init_cmds_size;
    } else {
        init_cmds = vendor_specific_init_default;
        init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(jd9261_lcd_init_cmd_t);
    }

    for (int i = 0; i < init_cmds_size; i++) {
        // Check if the command has been used or conflicts with the internal
        if (is_command1_enable && (init_cmds[i].data_bytes > 0)) {
            switch (init_cmds[i].cmd) {
            case LCD_CMD_MADCTL:
                is_cmd_overwritten = true;
                jd9261->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
                break;
            case LCD_CMD_COLMOD:
                is_cmd_overwritten = true;
                jd9261->colmod_val = ((uint8_t *)init_cmds[i].data)[0];
                break;
            default:
                is_cmd_overwritten = false;
                break;
            }

            if (is_cmd_overwritten) {
                is_cmd_overwritten = false;
                ESP_LOGW(TAG, "The %02Xh command has been used and will be overwritten by external initialization sequence",
                         init_cmds[i].cmd);
            }
        }

        // Send command
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));

        // Check if the current cmd is the command1 enable cmd
        if ((init_cmds[i].cmd == JD9261_PAGE_CMD2 || init_cmds[i].cmd == JD9261_PAGE_CMD3) && init_cmds[i].data_bytes > 0) {
            is_command1_enable = false;
        } else if (init_cmds[i].cmd == JD9261_PAGE_CMD1 && init_cmds[i].data_bytes > 0) {
            is_command1_enable = true;
        }
    }
    ESP_LOGD(TAG, "send init commands success");

    ESP_RETURN_ON_ERROR(jd9261->init(panel), TAG, "init MIPI DPI panel failed");

    return ESP_OK;
}

static esp_err_t panel_jd9261_reset(esp_lcd_panel_t *panel)
{
    jd9261_panel_t *jd9261 = (jd9261_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = jd9261->io;

    // Perform hardware reset
    if (jd9261->reset_gpio_num >= 0) {
        gpio_set_level(jd9261->reset_gpio_num, jd9261->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(jd9261->reset_gpio_num, !jd9261->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(120));
    } else if (io) { // Perform software reset
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    return ESP_OK;
}

static esp_err_t panel_jd9261_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    jd9261_panel_t *jd9261 = (jd9261_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = jd9261->io;
    uint8_t command = 0;

    ESP_RETURN_ON_FALSE(io, ESP_ERR_INVALID_STATE, TAG, "invalid panel IO");

    if (invert_color_data) {
        command = LCD_CMD_INVON;
    } else {
        command = LCD_CMD_INVOFF;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");

    return ESP_OK;
}

static esp_err_t panel_jd9261_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    jd9261_panel_t *jd9261 = (jd9261_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = jd9261->io;
    uint8_t madctl_val = jd9261->madctl_val;

    ESP_RETURN_ON_FALSE(io, ESP_ERR_INVALID_STATE, TAG, "invalid panel IO");

    // Control mirror through LCD command
    if (mirror_x) {
        madctl_val |= BIT(6);
    } else {
        madctl_val &= ~BIT(6);
    }
    if (mirror_y) {
        madctl_val |= BIT(7);
    } else {
        madctl_val &= ~BIT(7);
    }

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t []) {
        madctl_val
    }, 1), TAG, "send command failed");
    jd9261->madctl_val = madctl_val;

    return ESP_OK;
}

static esp_err_t panel_jd9261_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    jd9261_panel_t *jd9261 = (jd9261_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = jd9261->io;
    int command = 0;

    if (on_off) {
        command = LCD_CMD_DISPON;
    } else {
        command = LCD_CMD_DISPOFF;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}
#endif
