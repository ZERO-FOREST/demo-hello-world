set(LVGL_FONT_PATH "../../managed_components/lvgl__lvgl/src/font")

idf_component_register(SRCS "lv_port_indev.c" "lv_port_disp.c"
                            "${LVGL_FONT_PATH}/lv_font_montserrat_14.c"
                            "${LVGL_FONT_PATH}/lv_font_montserrat_16.c"
                            "${LVGL_FONT_PATH}/lv_font_montserrat_18.c"
                            "${LVGL_FONT_PATH}/lv_font_montserrat_20.c"
                            "${LVGL_FONT_PATH}/lv_font_montserrat_24.c"
                            "${LVGL_FONT_PATH}/lv_font_montserrat_32.c"
                    INCLUDE_DIRS "."
                    REQUIRES lvgl log Peripherals)

# 让LVGL找到我们的lv_conf.h配置文件
target_compile_definitions(${COMPONENT_LIB} PUBLIC LV_CONF_INCLUDE_SIMPLE) 