idf_component_register(SRCS "esp_lcd_jd9261.c" "esp_lcd_jd9261_general.c" "esp_lcd_jd9261_rgb.c" "esp_lcd_jd9261_mipi.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "priv_include"
                       REQUIRES "esp_lcd" "driver")