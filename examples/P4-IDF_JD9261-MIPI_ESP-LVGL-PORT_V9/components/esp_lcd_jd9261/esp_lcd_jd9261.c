/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "soc/soc_caps.h"
#include "esp_check.h"
#include "esp_lcd_types.h"

#include "jd9261_interface.h"
#include "esp_lcd_jd9261.h"

static const char *TAG = "jd9261";

esp_err_t esp_lcd_new_panel_jd9261(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                    esp_lcd_panel_handle_t *ret_panel)
{
    ESP_RETURN_ON_FALSE(panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    esp_err_t ret = ESP_ERR_NOT_SUPPORTED;

    if (panel_dev_config->vendor_config) {
        jd9261_vendor_config_t *vendor_config = (jd9261_vendor_config_t *)panel_dev_config->vendor_config;

        if (vendor_config->flags.use_rgb_interface + vendor_config->flags.use_qspi_interface +
                vendor_config->flags.use_mipi_interface > 1) {
            ESP_LOGE(TAG, "Only one interface is supported");
            return ESP_ERR_NOT_SUPPORTED;
        }

#if SOC_MIPI_DSI_SUPPORTED
        if (vendor_config->flags.use_mipi_interface) {
            ret = esp_lcd_new_panel_jd9261_mipi(io, panel_dev_config, ret_panel);

            return ret;
        }
#endif

#if SOC_LCD_RGB_SUPPORTED
        if (vendor_config->flags.use_rgb_interface) {
            ret = esp_lcd_new_panel_jd9261_rgb(io, panel_dev_config, ret_panel);

            return ret;
        }
#endif

        ret = esp_lcd_new_panel_jd9261_general(io, panel_dev_config, ret_panel);

        return ret;
    } else {
        ret = esp_lcd_new_panel_jd9261_general(io, panel_dev_config, ret_panel);
    }

    return ret;
}
