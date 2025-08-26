set(LVGL_FONT_PATH "../../managed_components/lvgl__lvgl/src/font")

set(LVGL_SRCS "")

if(NOT EN_RECEIVER_MODE)
    set(LVGL_SRCS
        "lv_port_indev.c"
        "lv_port_disp.c"
        "${LVGL_FONT_PATH}/lv_font_montserrat_12.c"
        "${LVGL_FONT_PATH}/lv_font_montserrat_14.c"
        "${LVGL_FONT_PATH}/lv_font_montserrat_16.c"
        "${LVGL_FONT_PATH}/lv_font_montserrat_18.c"
        "${LVGL_FONT_PATH}/lv_font_montserrat_20.c"
        "${LVGL_FONT_PATH}/lv_font_montserrat_24.c"
        "${LVGL_FONT_PATH}/lv_font_montserrat_32.c"
    )
idf_component_register(
    SRCS ${LVGL_SRCS}
    INCLUDE_DIRS "."
    REQUIRES lvgl log Peripherals
)

else()

idf_component_register(
    SRCS ${LVGL_SRCS}
    INCLUDE_DIRS "."
    REQUIRES log Peripherals
)

endif()

if(NOT EN_RECEIVER_MODE)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC LV_CONF_INCLUDE_SIMPLE)
    idf_build_set_property(COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}" APPEND)
endif()
