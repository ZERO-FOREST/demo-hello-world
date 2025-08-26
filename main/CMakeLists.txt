set(MAIN_SRCS
    "main.c"
    "task_init.c"
    "components_init.c"
    "demo/lsm6ds3_demo.c"
    "demo/i2s_tdm_demo.c"
    "UI/ui_start_animation.c"
    "UI/ui_main.c"
    "UI/ui_wifi_settings.c"
    "UI/ui_settings.c"
    "UI/ui_image_transfer.c"
    "UI/ui_serial_display.c"
    "UI/ui_calibration.c"
    "UI/ui_test.c"
    "UI/theme_manager.c"
    "UI/ui_common.c"
    "UI/ui_telemetry.c"
    "UI/ui_state_manager.c"
    "app/game/game_main.c"
    "app/game/game_tetris.c"
    "app/game/game_snake.c"
    "app/wifi_image_transfer.c"
    "app/p2p_udp_image_transfer.c"
    "app/serial_display.c"
    "app/calibration_manager.c"
    "app/lvgl_main.c"
    "app/power_management.c"
    "app/background_manager.c"
    "app/settings_manager.c"
    "app/Telemetry/src/telemetry_protocol.c"
    "app/Telemetry/src/telemetry_tcp.c"
    "app/Telemetry/src/telemetry_data_converter.c"
    "app/Telemetry/src/telemetry_main.c"
    "app/Telemetry/src/telemetry_receiver.c"
    "app/Telemetry/src/telemetry_sender.c"
    "fonts/font_init.c"
    "fonts/symbol.c"
)

if(NOT IDF_TARGET STREQUAL "esp32s3")
    list(APPEND MAIN_SRCS
        "app/bluetooth_audio_controller.c"
        "app/bluetooth_audio_demo.c"
        "app/bluetooth_audio_task.c"
    )
endif()

idf_component_register(
    SRCS ${MAIN_SRCS}
    INCLUDE_DIRS "inc" "UI/inc" "demo" "app/inc" "app/game" "fonts" "app/Telemetry/inc"
    REQUIRES lvgl esp_timer lvgl_port Peripherals driver spiffs spi_flash
)