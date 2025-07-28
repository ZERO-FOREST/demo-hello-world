idf_component_register(SRCS "lv_port_indev.c" "lv_port_disp.c"
                    INCLUDE_DIRS "."
                    REQUIRES lvgl log Peripherals)

# 让LVGL找到我们的lv_conf.h配置文件
target_compile_definitions(${COMPONENT_LIB} PUBLIC LV_CONF_INCLUDE_SIMPLE) 