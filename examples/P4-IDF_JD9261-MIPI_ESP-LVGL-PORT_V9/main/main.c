#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2c.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "driver/gpio.h"
#include "esp_ldo_regulator.h"
#include "esp_lvgl_port.h"
#include "lv_demos.h"

#include "esp_lcd_jd9261.h"
#include "esp_lcd_touch_jd9261.h"

/* LCD size */
#define EXAMPLE_LCD_H_RES   (424)
#define EXAMPLE_LCD_V_RES   (1280)

#if LV_COLOR_DEPTH == 16
#define MIPI_DPI_PX_FORMAT (LCD_COLOR_PIXEL_FORMAT_RGB565)
#define BSP_LCD_COLOR_DEPTH (16)
#define LV_COLOR_FORMAT LV_COLOR_FORMAT_RGB565
#elif LV_COLOR_DEPTH == 24
#define MIPI_DPI_PX_FORMAT (LCD_COLOR_PIXEL_FORMAT_RGB888)
#define BSP_LCD_COLOR_DEPTH (24)
#define LV_COLOR_FORMAT LV_COLOR_FORMAT_RGB888
#endif

// “VDD_MIPI_DPHY”应供电 2.5V，可从内部 LDO 稳压器或外部 LDO 芯片获取电源
#define EXAMPLE_MIPI_DSI_PHY_PWR_LDO_CHAN 3 // LDO_VO3 连接至 VDD_MIPI_DPHY
#define EXAMPLE_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV 2500
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL 1
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL
#define EXAMPLE_PIN_NUM_BK_LIGHT -1
#define EXAMPLE_PIN_NUM_LCD_RST  -1

#define CONFIG_EXAMPLE_LCD_TOUCH_ENABLED 0 //是否启动触摸

#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
/* Touch settings */
#define EXAMPLE_TOUCH_I2C_NUM       (0)
#define EXAMPLE_TOUCH_I2C_CLK_HZ    (400000)

/* LCD touch pins */
#define EXAMPLE_PIN_NUM_TOUCH_SCL      (GPIO_NUM_8)
#define EXAMPLE_PIN_NUM_TOUCH_SDA      (GPIO_NUM_7)
#define EXAMPLE_PIN_NUM_TOUCH_RST      (GPIO_NUM_NC)
#define EXAMPLE_PIN_NUM_TOUCH_INT      (GPIO_NUM_NC)
#endif

static const char *TAG = "EXAMPLE";

/* LCD IO and panel */
static esp_lcd_panel_handle_t lcd_panel = NULL;
static esp_lcd_panel_io_handle_t io_handle = NULL;
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
static esp_lcd_touch_handle_t touch_handle = NULL;
#endif

/* LVGL display and touch */
static lv_display_t *lvgl_disp = NULL;
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
static lv_indev_t *lvgl_touch_indev = NULL;
#endif

static void example_bsp_enable_dsi_phy_power(void)
{
    // 打开 MIPI DSI PHY 的电源，使其从“无电”状态进入“开机”状态
    esp_ldo_channel_handle_t ldo_mipi_phy = NULL;
#ifdef EXAMPLE_MIPI_DSI_PHY_PWR_LDO_CHAN
    esp_ldo_channel_config_t ldo_mipi_phy_config = {
        .chan_id = EXAMPLE_MIPI_DSI_PHY_PWR_LDO_CHAN,
        .voltage_mv = EXAMPLE_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_mipi_phy_config, &ldo_mipi_phy));
    ESP_LOGI(TAG, "MIPI DSI PHY Powered on");
#endif
}

static void example_bsp_init_lcd_backlight(void)
{
#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_BK_LIGHT};
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
#endif
}

static void example_bsp_set_lcd_backlight(uint32_t level)
{
#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, level);
#endif
}

static const jd9261_lcd_init_cmd_t lcd_init_cmds[] = {
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

static esp_err_t app_lcd_init(void)
{
    esp_err_t ret = ESP_OK;

    example_bsp_enable_dsi_phy_power();
    example_bsp_init_lcd_backlight();
    example_bsp_set_lcd_backlight(EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL);

    // 首先创建 MIPI DSI 总线，它还将初始化 DSI PHY
    esp_lcd_dsi_bus_handle_t mipi_dsi_bus;
    esp_lcd_dsi_bus_config_t bus_config = {                    \
        .bus_id = 0,                                           \
        .num_data_lanes = 2,                                   \
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,           \
        .lane_bit_rate_mbps = 630,                             \
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus), err, TAG, "LCD init failed");

    ESP_LOGI(TAG, "Install MIPI DSI LCD control panel");
    // 我们使用DBI接口发送LCD命令和参数
    esp_lcd_dbi_io_config_t dbi_config = JD9261_MIPI_PANEL_IO_DBI_CONFIG();

    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &io_handle), err, TAG, "LCD init failed");

    // 创建JD9261控制面板
    esp_lcd_dpi_panel_config_t dpi_config = {                 \
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,          \
        .dpi_clock_freq_mhz = 45.4,                           \
        .virtual_channel = 0,                                 \
        .pixel_format = MIPI_DPI_PX_FORMAT,                   \
        .num_fbs = 1,                                         \
        .video_timing = {                                     \
            .h_size = EXAMPLE_LCD_H_RES,                      \
            .v_size = EXAMPLE_LCD_V_RES,                      \
            .hsync_back_porch = 40,                           \
            .hsync_pulse_width = 20,                          \
            .hsync_front_porch = 20,                          \
            .vsync_back_porch = 19,                           \
            .vsync_pulse_width = 2,                           \
            .vsync_front_porch = 200,                         \
        },                                                    \
        .flags.use_dma2d = true,                              \
    };

    jd9261_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,      // Uncomment these line if use custom initialization commands
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(jd9261_lcd_init_cmd_t),
        .flags.use_mipi_interface = 1,
        .mipi_config = {
            .dsi_bus = mipi_dsi_bus,
            .dpi_config = &dpi_config,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = BSP_LCD_COLOR_DEPTH,
        .vendor_config = &vendor_config,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_jd9261(io_handle, &panel_config, &lcd_panel), err, TAG, "LCD init failed");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_reset(lcd_panel), err, TAG, "LCD init failed");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_init(lcd_panel), err, TAG, "LCD init failed");

    // 打开背光
    example_bsp_set_lcd_backlight(EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);

    return ret;

err:
    if (lcd_panel) {
        esp_lcd_panel_del(lcd_panel);
    }
    return ret;
}

#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
static esp_err_t app_touch_init(void)
{
    /* Initilize I2C */
    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = EXAMPLE_PIN_NUM_TOUCH_SDA,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_io_num = EXAMPLE_PIN_NUM_TOUCH_SCL,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = EXAMPLE_TOUCH_I2C_CLK_HZ
    };
    ESP_RETURN_ON_ERROR(i2c_param_config(EXAMPLE_TOUCH_I2C_NUM, &i2c_conf), TAG, "I2C configuration failed");
    ESP_RETURN_ON_ERROR(i2c_driver_install(EXAMPLE_TOUCH_I2C_NUM, i2c_conf.mode, 0, 0, 0), TAG, "I2C initialization failed");

    /* Initialize touch HW */
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = EXAMPLE_LCD_H_RES,
        .y_max = EXAMPLE_LCD_V_RES,
        .rst_gpio_num = EXAMPLE_PIN_NUM_TOUCH_RST,
        .int_gpio_num = EXAMPLE_PIN_NUM_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_JD9261_CONFIG();
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)EXAMPLE_TOUCH_I2C_NUM, &tp_io_config, &tp_io_handle), TAG, "");
    return esp_lcd_touch_new_i2c_jd9261(tp_io_handle, &tp_cfg, &touch_handle);
}
#endif

static esp_err_t app_lvgl_init(void)
{
    /* Initialize LVGL */
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port initialization failed");

    /* Add LCD screen */
    ESP_LOGD(TAG, "Add LCD screen");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = lcd_panel,
        .buffer_size = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES,
        .double_buffer = true,
        .hres = EXAMPLE_LCD_H_RES,
        .vres = EXAMPLE_LCD_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
            .sw_rotate = true,
            .swap_bytes = false,
            .full_refresh = false,
            .direct_mode = false,
        }
    };

    const lvgl_port_display_dsi_cfg_t dpi_cfg = {
        .flags = {
#if CONFIG_BSP_DISPLAY_LVGL_AVOID_TEAR
            .avoid_tearing = true,
#else
            .avoid_tearing = false,
#endif
        }
    };

    lvgl_disp = lvgl_port_add_disp_dsi(&disp_cfg, &dpi_cfg);

#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
    /* Add touch input (for selected screen) */
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = lvgl_disp,
        .handle = touch_handle,
    };
    lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);
#endif

    return ESP_OK;
}

void app_main(void)
{
    /* LCD HW initialization */
    ESP_ERROR_CHECK(app_lcd_init());

#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
    /* Touch initialization */
    ESP_ERROR_CHECK(app_touch_init());
#endif

    /* LVGL initialization */
    ESP_ERROR_CHECK(app_lvgl_init());

    /* Show LVGL objects */
    lvgl_port_lock(0);

    // lv_demo_music();
    lv_demo_widgets();

    lvgl_port_unlock();
}
