#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tcp_protocol.h"

static const char* TAG = "cmd_terminal";

// 输出回调：上层可在其他源文件中提供强符号实现以覆写（例如通过USB CDC回传）
__attribute__((weak)) void cmd_terminal_write(const char* s) {
    if (s) {
        ESP_LOGI(TAG, "%s", s);
    }
}

// 运行时设置JPEG质量的钩子：jpeg模块可提供强符号实现，默认仅提示未生效
__attribute__((weak)) bool cmd_terminal_set_jpeg_quality(uint8_t quality) {
    ESP_LOGW(TAG, "未实现运行时修改JPEG质量，期望质量=%u (需要在jpeg模块中实现覆盖)", quality);
    return false;
}

static void respondf(const char* fmt, ...) {
    char buf[192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    cmd_terminal_write(buf);
}

static void str_tolower(char* s) {
    if (!s) return;
    for (; *s; ++s) {
        *s = (char)tolower((unsigned char)*s);
    }
}

static void trim_trailing_newline(char* s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
}

static void handle_text_command(char* line) {
    if (!line) return;

    trim_trailing_newline(line);

    // 为不区分大小写比较准备副本
    char cmd_copy[160];
    strncpy(cmd_copy, line, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';
    str_tolower(cmd_copy);

    // 第一个token作为命令
    char* saveptr = NULL;
    char* cmd = strtok_r(cmd_copy, " \t", &saveptr);
    if (!cmd) return;

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
        respondf("可用命令:\n"
                 "  help                - 显示帮助\n"
                 "  heap                - 打印空闲堆内存\n"
                 "  tasks               - 打印任务数量\n"
                 "  version             - 打印IDF版本\n"
                 "  echo <text>         - 回显文本\n"
                 "  jpegq <0-100>       - 设置JPEG质量(需要模块支持)");
        return;
    }

    if (strcmp(cmd, "heap") == 0) {
        respondf("Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
        return;
    }

    if (strcmp(cmd, "tasks") == 0) {
        respondf("Tasks: %u", (unsigned)uxTaskGetNumberOfTasks());
        return;
    }

    if (strcmp(cmd, "version") == 0) {
        respondf("IDF: %s", esp_get_idf_version());
        return;
    }

    if (strncmp(cmd, "echo", 4) == 0) {
        // 保留原始大小写与空格：在原始行中找第一个空格后的内容
        const char* p = strchr(line, ' ');
        if (p) {
            while (*p == ' ' || *p == '\t') p++;
            respondf("%s", p);
        } else {
            respondf("");
        }
        return;
    }

    if (strncmp(cmd, "jpegq", 5) == 0) {
        const char* p = strchr(line, ' ');
        if (!p) {
            respondf("用法: jpegq <0-100>");
            return;
        }
        while (*p == ' ' || *p == '\t') p++;
        int q = atoi(p);
        if (q < 0) q = 0;
        if (q > 100) q = 100;
        bool ok = cmd_terminal_set_jpeg_quality((uint8_t)q);
        respondf("设置JPEG质量=%d, %s", q, ok ? "成功" : "未生效(需模块支持)");
        return;
    }

    respondf("未知命令: %s (输入 help 获取帮助)", line);
}

// 覆盖弱符号：解析来自USB/SPI的扩展命令
void handle_extended_command(const extended_cmd_payload_t* cmd_data) {
    if (!cmd_data) return;

    // 约定: cmd_id == 0x01 表示文本终端命令，params中为ASCII命令行
    if (cmd_data->cmd_id == 0x01) {
        size_t len = cmd_data->param_len;
        if (len > sizeof(cmd_data->params)) {
            len = sizeof(cmd_data->params);
        }
        char line[sizeof(cmd_data->params) + 1];
        memcpy(line, cmd_data->params, len);
        line[len] = '\0';
        handle_text_command(line);
    } else {
        ESP_LOGI(TAG, "收到扩展命令: id=0x%02X, len=%u", cmd_data->cmd_id, (unsigned)cmd_data->param_len);
    }
}

// 新增：对外暴露的行命令处理接口（用于USB CDC等直接文本输入）
void cmd_terminal_handle_line(const char* line) {
    if (!line) return;
    // 复制到可修改缓冲区，便于去除换行与分词
    char buf[192];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    handle_text_command(buf);
}