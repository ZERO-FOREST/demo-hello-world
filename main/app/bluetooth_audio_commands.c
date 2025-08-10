/**
 * @file bluetooth_audio_commands.c
 * @brief 蓝牙音频控制命令实现
 * @author Your Name
 * @date 2024
 */

#include "bluetooth_audio_commands.h"
#include "bluetooth_audio_task.h"
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"

static const char* TAG = "BT_AUDIO_CMD";

// ========================================
// 控制台命令结构体
// ========================================
static struct {
    struct arg_int *volume;
    struct arg_end *end;
} volume_args;

static struct {
    struct arg_lit *discoverable;
    struct arg_end *end;
} discoverable_args;

// ========================================
// 命令处理函数
// ========================================

/**
 * @brief 显示蓝牙音频状态
 */
static int cmd_bt_audio_status(int argc, char **argv)
{
    bt_audio_status_t status;
    esp_err_t ret = bluetooth_audio_task_get_status(&status);
    
    if (ret != ESP_OK) {
        printf("Failed to get status: %s\n", esp_err_to_name(ret));
        return 1;
    }
    
    printf("=== Bluetooth Audio Status ===\n");
    printf("Task Running:     %s\n", status.task_running ? "YES" : "NO");
    printf("BT Initialized:   %s\n", status.bt_initialized ? "YES" : "NO");
    printf("Device Connected: %s\n", status.bt_connected ? "YES" : "NO");
    printf("Audio Playing:    %s\n", status.bt_playing ? "YES" : "NO");
    printf("Current Volume:   %d%%\n", status.current_volume);
    printf("Total Frames:     %d\n", status.total_frames);
    printf("Dropped Frames:   %d\n", status.dropped_frames);
    printf("Connections:      %d\n", status.connection_count);
    
    if (status.total_frames > 0) {
        float drop_rate = ((float)status.dropped_frames / status.total_frames) * 100.0f;
        printf("Drop Rate:        %.2f%%\n", drop_rate);
    }
    
    printf("==============================\n");
    
    return 0;
}

/**
 * @brief 设置音量
 */
static int cmd_bt_audio_volume(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &volume_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, volume_args.end, argv[0]);
        return 1;
    }
    
    int volume = volume_args.volume->ival[0];
    if (volume < 0 || volume > 100) {
        printf("Volume must be between 0 and 100\n");
        return 1;
    }
    
    esp_err_t ret = bluetooth_audio_task_set_volume((uint8_t)volume);
    if (ret == ESP_OK) {
        printf("Volume set to %d%%\n", volume);
    } else {
        printf("Failed to set volume: %s\n", esp_err_to_name(ret));
        return 1;
    }
    
    return 0;
}

/**
 * @brief 重置统计信息
 */
static int cmd_bt_audio_reset(int argc, char **argv)
{
    esp_err_t ret = bluetooth_audio_task_reset_stats();
    if (ret == ESP_OK) {
        printf("Statistics reset successfully\n");
    } else {
        printf("Failed to reset statistics: %s\n", esp_err_to_name(ret));
        return 1;
    }
    
    return 0;
}

/**
 * @brief 设置可发现模式
 */
static int cmd_bt_audio_discoverable(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &discoverable_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, discoverable_args.end, argv[0]);
        return 1;
    }
    
    bool discoverable = discoverable_args.discoverable->count > 0;
    
    esp_err_t ret = bluetooth_audio_task_set_discoverable(discoverable);
    if (ret == ESP_OK) {
        printf("Discoverable mode: %s\n", discoverable ? "ON" : "OFF");
    } else {
        printf("Failed to set discoverable mode: %s\n", esp_err_to_name(ret));
        return 1;
    }
    
    return 0;
}

/**
 * @brief 蓝牙音频帮助信息
 */
static int cmd_bt_audio_help(int argc, char **argv)
{
    printf("Bluetooth Audio Commands:\n");
    printf("  bt_status           - Show current status\n");
    printf("  bt_volume <0-100>   - Set volume (0-100)\n");
    printf("  bt_reset            - Reset statistics\n");
    printf("  bt_discoverable     - Enable discoverable mode\n");
    printf("  bt_hidden           - Disable discoverable mode\n");
    printf("  bt_help             - Show this help\n");
    printf("\nExample usage:\n");
    printf("  bt_volume 50        - Set volume to 50%%\n");
    printf("  bt_discoverable     - Make device discoverable\n");
    printf("  bt_status           - Check current status\n");
    
    return 0;
}

// ========================================
// 公共函数实现
// ========================================

esp_err_t bluetooth_audio_commands_register(void)
{
    // 准备命令参数结构
    volume_args.volume = arg_int1(NULL, NULL, "<volume>", "Volume level (0-100)");
    volume_args.end = arg_end(2);
    
    discoverable_args.discoverable = arg_lit0("d", "discoverable", "Enable discoverable mode");
    discoverable_args.end = arg_end(2);
    
    // 注册状态查看命令
    const esp_console_cmd_t status_cmd = {
        .command = "bt_status",
        .help = "Show Bluetooth audio status",
        .hint = NULL,
        .func = &cmd_bt_audio_status,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&status_cmd));
    
    // 注册音量控制命令
    const esp_console_cmd_t volume_cmd = {
        .command = "bt_volume",
        .help = "Set Bluetooth audio volume (0-100)",
        .hint = NULL,
        .func = &cmd_bt_audio_volume,
        .argtable = &volume_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&volume_cmd));
    
    // 注册统计重置命令
    const esp_console_cmd_t reset_cmd = {
        .command = "bt_reset",
        .help = "Reset Bluetooth audio statistics",
        .hint = NULL,
        .func = &cmd_bt_audio_reset,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&reset_cmd));
    
    // 注册可发现模式命令
    const esp_console_cmd_t discoverable_cmd = {
        .command = "bt_discoverable",
        .help = "Enable Bluetooth discoverable mode",
        .hint = NULL,
        .func = &cmd_bt_audio_discoverable,
        .argtable = &discoverable_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&discoverable_cmd));
    
    // 注册隐藏模式命令
    const esp_console_cmd_t hidden_cmd = {
        .command = "bt_hidden",
        .help = "Disable Bluetooth discoverable mode",
        .hint = NULL,
        .func = &cmd_bt_audio_discoverable,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&hidden_cmd));
    
    // 注册帮助命令
    const esp_console_cmd_t help_cmd = {
        .command = "bt_help",
        .help = "Show Bluetooth audio help",
        .hint = NULL,
        .func = &cmd_bt_audio_help,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&help_cmd));
    
    ESP_LOGI(TAG, "Bluetooth Audio console commands registered");
    
    return ESP_OK;
}