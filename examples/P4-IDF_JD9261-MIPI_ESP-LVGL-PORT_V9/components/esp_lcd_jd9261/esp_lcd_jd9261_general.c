/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <sys/cdefs.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "esp_log.h"
#include "esp_lcd_jd9261.h"
#include "jd9261_interface.h"

static const char *TAG = "jd9261_general";

static esp_err_t panel_jd9261_del(esp_lcd_panel_t *panel);
static esp_err_t panel_jd9261_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_jd9261_init(esp_lcd_panel_t *panel);
static esp_err_t panel_jd9261_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
static esp_err_t panel_jd9261_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_jd9261_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_jd9261_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_jd9261_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_jd9261_disp_on_off(esp_lcd_panel_t *panel, bool off);

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    int x_gap;
    int y_gap;
    uint8_t fb_bits_per_pixel;
    uint8_t madctl_val; // save current value of LCD_CMD_MADCTL register
    uint8_t colmod_val; // save surrent value of LCD_CMD_COLMOD register
    const jd9261_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    struct {
        unsigned int use_qspi_interface: 1;
        unsigned int reset_level: 1;
    } flags;
} jd9261_panel_t;

esp_err_t esp_lcd_new_panel_jd9261_general(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel)
{
    ESP_RETURN_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    esp_err_t ret = ESP_OK;
    jd9261_panel_t *jd9261 = NULL;
    jd9261 = calloc(1, sizeof(jd9261_panel_t));
    ESP_GOTO_ON_FALSE(jd9261, ESP_ERR_NO_MEM, err, TAG, "no mem for jd9261 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    switch (panel_dev_config->rgb_ele_order) {
    case LCD_RGB_ELEMENT_ORDER_RGB:
        jd9261->madctl_val = 0;
        break;
    case LCD_RGB_ELEMENT_ORDER_BGR:
        jd9261->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color element order");
        break;
    }

    switch (panel_dev_config->bits_per_pixel) {
    case 16: // RGB565
        jd9261->colmod_val = 0x55;
        jd9261->fb_bits_per_pixel = 16;
        break;
    case 18: // RGB666
        jd9261->colmod_val = 0x66;
        // each color component (R/G/B) should occupy the 6 high bits of a byte, which means 3 full bytes are required for a pixel
        jd9261->fb_bits_per_pixel = 24;
        break;
    case 24: // RGB888
        jd9261->colmod_val = 0x77;
        jd9261->fb_bits_per_pixel = 24;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    jd9261->io = io;
    jd9261->reset_gpio_num = panel_dev_config->reset_gpio_num;
    jd9261->flags.reset_level = panel_dev_config->flags.reset_active_high;
    jd9261_vendor_config_t *vendor_config = (jd9261_vendor_config_t *)panel_dev_config->vendor_config;
    if (vendor_config) {
        jd9261->init_cmds = vendor_config->init_cmds;
        jd9261->init_cmds_size = vendor_config->init_cmds_size;
        jd9261->flags.use_qspi_interface = vendor_config->flags.use_qspi_interface;
    }
    jd9261->base.del = panel_jd9261_del;
    jd9261->base.reset = panel_jd9261_reset;
    jd9261->base.init = panel_jd9261_init;
    jd9261->base.draw_bitmap = panel_jd9261_draw_bitmap;
    jd9261->base.invert_color = panel_jd9261_invert_color;
    jd9261->base.set_gap = panel_jd9261_set_gap;
    jd9261->base.mirror = panel_jd9261_mirror;
    jd9261->base.swap_xy = panel_jd9261_swap_xy;
    jd9261->base.disp_on_off = panel_jd9261_disp_on_off;
    *ret_panel = &(jd9261->base);
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

static esp_err_t tx_param(jd9261_panel_t *jd9261, esp_lcd_panel_io_handle_t io, int lcd_cmd, const void *param, size_t param_size)
{
    if (jd9261->flags.use_qspi_interface) {
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;
    }
    return esp_lcd_panel_io_tx_param(io, lcd_cmd, param, param_size);
}

static esp_err_t tx_color(jd9261_panel_t *jd9261, esp_lcd_panel_io_handle_t io, int lcd_cmd, const void *param, size_t param_size)
{
    if (jd9261->flags.use_qspi_interface) {
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= LCD_OPCODE_WRITE_COLOR << 24;
    }
    return esp_lcd_panel_io_tx_color(io, lcd_cmd, param, param_size);
}

static esp_err_t panel_jd9261_del(esp_lcd_panel_t *panel)
{
    jd9261_panel_t *jd9261 = __containerof(panel, jd9261_panel_t, base);

    if (jd9261->reset_gpio_num >= 0) {
        gpio_reset_pin(jd9261->reset_gpio_num);
    }
    ESP_LOGD(TAG, "del jd9261 panel @%p", jd9261);
    free(jd9261);
    return ESP_OK;
}

static esp_err_t panel_jd9261_reset(esp_lcd_panel_t *panel)
{
    jd9261_panel_t *jd9261 = __containerof(panel, jd9261_panel_t, base);
    esp_lcd_panel_io_handle_t io = jd9261->io;

    // Perform hardware reset
    if (jd9261->reset_gpio_num >= 0) {
        gpio_set_level(jd9261->reset_gpio_num, jd9261->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(jd9261->reset_gpio_num, !jd9261->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(120));
    } else { // Perform software reset
        ESP_RETURN_ON_ERROR(tx_param(jd9261, io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    return ESP_OK;
}

static const jd9261_lcd_init_cmd_t vendor_specific_init_default[] = {
//  {cmd, { data }, data_size, delay_ms}
    {0x99, (uint8_t []){0x71,0x02,0xa2}, 3, 0},
    {0x99, (uint8_t []){0x71,0x02,0xa3}, 3, 0},
    {0x99, (uint8_t []){0x71,0x02,0xa4}, 3, 0},
   	{0xA4, (uint8_t []){0x31}, 1, 0},
    {0xB0, (uint8_t []){0x22,0x61,0x1E,0x61,0x2F,0x39,0x39}, 7, 0},
    {0xB7, (uint8_t []){0x46,0x46}, 2, 0},
    {0xBF, (uint8_t []){0x50,0x50}, 2, 0},
    {0xD7, (uint8_t []){0x00,0x10,0x8C,0x08,0xF0,0xF0}, 6, 0},
    {0xA3, (uint8_t []){0x40,0x03,0x8C,0x40,0x45,0x00,0x00,0x00,0x00,0x00,
                        0x1E,0x01,0x00,0x12,0x00,0x45,0x05,0x00,0x00,0x00,
                        0x00,0x1E,0x01,0x00,0x12,0x20,0x52,0x00,0x05,0x00,
                        0x00,0xFF}, 32, 0},
    {0xA6, (uint8_t []){0x08,0x00,0x24,0x55,0x35,0x00,0x76,0x40,0x4E,0x4E,
                        0x00,0x24,0x55,0x00,0x00,0x40,0x40,0x4E,0x4E,0x02,
                        0xAC,0x51,0x00,0xCC,0x40,0x40,0x4E,0x4E,0x00,0xAC,
                        0x11,0x00,0x00,0x40,0x40,0x4E,0x4E,0x00,0x00,0x06,
                        0x00,0x00,0x00,0x00}, 44, 0},
    {0xA7, (uint8_t []){0x19,0x19,0x00,0x64,0x40,0x07,0x16,0x40,0x00,0x44,
                        0x43,0x4E,0x4E,0x00,0x64,0x40,0x25,0x34,0x00,0x00,
                        0x42,0x41,0x4E,0x4E,0x00,0x64,0x40,0x4B,0x5A,0x00,
                        0x00,0x42,0x41,0x4E,0x4E,0x00,0x24,0x40,0x69,0x78,
                        0x00,0x00,0x40,0x40,0x4E,0x4E,0x00,0x44}, 48, 0},
    {0xAC, (uint8_t []){0x00,0x1C,0x04,0x1A,0x19,0x1B,0x1B,0x18,0x06,0x13,
                        0x19,0x11,0x1B,0x08,0x18,0x0A,0x01,0x1C,0x04,0x1A,
                        0x19,0x1B,0x1B,0x18,0x06,0x12,0x19,0x10,0x1B,0x09,
                        0x18,0x0B,0xBF,0xAA,0xBF,0xAA,0x00}, 37, 0},
    {0xAD, (uint8_t []){0xCC,0x40,0x46,0x11,0x04,0x6F,0x6F}, 7, 0}, 
    {0xE8, (uint8_t []){0x30,0x07,0x05,0x6A,0x6A,0x9C,0x00,0xE2,0x04,0x00,
                        0x00,0x00,0x00,0xEF}, 14, 0},
    {0x75, (uint8_t []){0x03,0x04}, 2, 0},
    {0xE7, (uint8_t []){0x8B,0x3C,0x00,0x0C,0xF0,0x5D,0x00,0x5D,0x00,0x5D,
                        0x00,0x5D,0x00,0xFF,0x00,0x08,0x7B,0x00,0x00,0xC8,
                        0x6A,0x5A,0x08,0x1A,0x3C,0x00,0xA1,0x01,0x8C,0x01,
                        0x7F,0xF0,0x22}, 33, 0},
    {0xE9, (uint8_t []){0x3C,0x7F,0x08,0x10,0x1A,0x7A,0x22,0x1A,0x33}, 9, 0},
    {0xC8, (uint8_t []){0x00,0x00,0x15,0x26,0x44,0x00,0x78,0x03,0xBE,0x06,
                        0x11,0x1C,0x09,0x8A,0x03,0x21,0xD4,0x01,0x11,0x0F,
                        0x22,0x4A,0x0F,0x8F,0x0A,0x32,0xF0,0x0A,0x41,0x0D,
                        0xF3,0x80,0x0D,0xAE,0xC5,0x03,0xC4}, 37, 0},
    {0xC9, (uint8_t []){0x00,0x00,0x15,0x26,0x44,0x00,0x78,0x03,0xBE,0x06,
                        0x11,0x1C,0x09,0x8A,0x03,0x21,0xD4,0x01,0x11,0x0F,
                        0x22,0x4A,0x0F,0x8F,0x0A,0x32,0xF0,0x0A,0x41,0x0D,
                        0xF3,0x80,0x0D,0xAE,0xC5,0x03,0xC4}, 37, 0},
    {0x99, (uint8_t []){0x71,0x02,0x00}, 3, 0},  
    {0x11, (uint8_t []){0x00}, 0, 120},
    {0x29, (uint8_t []){0x00}, 0, 20},
	{0x35, (uint8_t []){0x00}, 1, 0},
    {0x36, (uint8_t []){0x00}, 1, 0},
};

static esp_err_t panel_jd9261_init(esp_lcd_panel_t *panel)
{
    jd9261_panel_t *jd9261 = __containerof(panel, jd9261_panel_t, base);
    esp_lcd_panel_io_handle_t io = jd9261->io;
    const jd9261_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;
    bool is_command1_enable = true;
    bool is_cmd_overwritten = false;

    ESP_RETURN_ON_ERROR(tx_param(jd9261, io, LCD_CMD_MADCTL, (uint8_t[]) {
        jd9261->madctl_val,
    }, 1), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(tx_param(jd9261, io, LCD_CMD_COLMOD, (uint8_t[]) {
        jd9261->colmod_val,
    }, 1), TAG, "send command failed");

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
                ESP_LOGW(TAG, "The %02Xh command has been used and will be overwritten by external initialization sequence", init_cmds[i].cmd);
            }
        }

        // Send command
        ESP_RETURN_ON_ERROR(tx_param(jd9261, io, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));

        // Check if the current cmd is the command1 enable cmd
        if ((init_cmds[i].cmd == JD9261_PAGE_CMD2 || init_cmds[i].cmd == JD9261_PAGE_CMD3) && init_cmds[i].data_bytes > 0) {
            is_command1_enable = false;
        } else if (init_cmds[i].cmd == JD9261_PAGE_CMD1 && init_cmds[i].data_bytes > 0) {
            is_command1_enable = true;
        }
    }
    ESP_LOGD(TAG, "send init commands success");

    return ESP_OK;
}

static esp_err_t panel_jd9261_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    jd9261_panel_t *jd9261 = __containerof(panel, jd9261_panel_t, base);
    assert((x_start < x_end) && (y_start < y_end) && "start position must be smaller than end position");
    esp_lcd_panel_io_handle_t io = jd9261->io;

    x_start += jd9261->x_gap;
    x_end += jd9261->x_gap;
    y_start += jd9261->y_gap;
    y_end += jd9261->y_gap;

    // define an area of frame memory where MCU can access
    ESP_RETURN_ON_ERROR(tx_param(jd9261, io, LCD_CMD_CASET, (uint8_t[]) {
        (x_start >> 8) & 0xFF,
        x_start & 0xFF,
        ((x_end - 1) >> 8) & 0xFF,
        (x_end - 1) & 0xFF,
    }, 4), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(tx_param(jd9261, io, LCD_CMD_RASET, (uint8_t[]) {
        (y_start >> 8) & 0xFF,
        y_start & 0xFF,
        ((y_end - 1) >> 8) & 0xFF,
        (y_end - 1) & 0xFF,
    }, 4), TAG, "send command failed");
    // transfer frame buffer
    size_t len = (x_end - x_start) * (y_end - y_start) * jd9261->fb_bits_per_pixel / 8;
    tx_color(jd9261, io, LCD_CMD_RAMWR, color_data, len);

    return ESP_OK;
}

static esp_err_t panel_jd9261_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    jd9261_panel_t *jd9261 = __containerof(panel, jd9261_panel_t, base);
    esp_lcd_panel_io_handle_t io = jd9261->io;
    int command = 0;
    if (invert_color_data) {
        command = LCD_CMD_INVON;
    } else {
        command = LCD_CMD_INVOFF;
    }
    ESP_RETURN_ON_ERROR(tx_param(jd9261, io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_jd9261_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    jd9261_panel_t *jd9261 = __containerof(panel, jd9261_panel_t, base);
    esp_lcd_panel_io_handle_t io = jd9261->io;
    esp_err_t ret = ESP_OK;

    if (mirror_x) {
        jd9261->madctl_val |= BIT(6);
    } else {
        jd9261->madctl_val &= ~BIT(6);
    }
    if (mirror_y) {
        jd9261->madctl_val |= BIT(7);
    } else {
        jd9261->madctl_val &= ~BIT(7);
    }
    ESP_RETURN_ON_ERROR(tx_param(jd9261, io, LCD_CMD_MADCTL, (uint8_t[]) {
        jd9261->madctl_val
    }, 1), TAG, "send command failed");
    return ret;
}

static esp_err_t panel_jd9261_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    ESP_LOGW(TAG, "swap_xy is not supported by this panel");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t panel_jd9261_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    jd9261_panel_t *jd9261 = __containerof(panel, jd9261_panel_t, base);
    jd9261->x_gap = x_gap;
    jd9261->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_jd9261_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    jd9261_panel_t *jd9261 = __containerof(panel, jd9261_panel_t, base);
    esp_lcd_panel_io_handle_t io = jd9261->io;
    int command = 0;

    if (on_off) {
        command = LCD_CMD_DISPON;
    } else {
        command = LCD_CMD_DISPOFF;
    }
    ESP_RETURN_ON_ERROR(tx_param(jd9261, io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}
