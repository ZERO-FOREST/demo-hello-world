/*
 * @Author: ESP32 Demo
 * @Date: 2025-01-12
 * @Description: SPIFFS文件系统读写测试
 */

#include "spiffs_test_demo.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

static const char* TAG = "SPIFFS_TEST";

/**
 * @brief 初始化SPIFFS文件系统
 * @return esp_err_t ESP_OK成功，其他错误码失败
 */
esp_err_t spiffs_init(void) {
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "spiffs", // 明确指定分区标签
        .max_files = 5,
        .format_if_mount_failed = false // 关闭自动格式化，避免丢失数据
    };

    // 挂载SPIFFS分区
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    // 获取SPIFFS分区信息
    size_t total = 0, used = 0;
    ret = esp_spiffs_info("spiffs", &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        esp_vfs_spiffs_unregister("spiffs");
        return ret;
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    // 检查SPIFFS分区是否正常工作
    if (used > total) {
        ESP_LOGW(TAG, "Number of used bytes cannot be larger than total. Performing SPIFFS check.");
        ret = esp_spiffs_check("spiffs");
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPIFFS check failed (%s)", esp_err_to_name(ret));
            return ret;
        } else {
            ESP_LOGI(TAG, "SPIFFS check successful");
        }
    }

    ESP_LOGI(TAG, "SPIFFS mounted successfully");
    return ESP_OK;
}

/**
 * @brief 测试读取font_noto_sans_sc_16_2bpp.bin文件
 * @return esp_err_t ESP_OK成功，其他错误码失败
 */
esp_err_t test_read_font_file(void) {
    ESP_LOGI(TAG, "Testing font file reading...");

    const char* font_path = "/spiffs/font_noto_sans_sc_16_2bpp.bin";
    FILE* f = fopen(font_path, "rb");

    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open font file: %s", font_path);
        return ESP_FAIL;
    }

    // 获取文件大小
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    ESP_LOGI(TAG, "Font file size: %ld bytes", file_size);

    if (file_size <= 0) {
        ESP_LOGW(TAG, "Font file is empty or invalid");
        fclose(f);
        return ESP_FAIL;
    }

    // 读取文件前256字节作为测试
    uint8_t buffer[256];
    size_t bytes_to_read = (file_size < 256) ? file_size : 256;
    size_t bytes_read = fread(buffer, 1, bytes_to_read, f);

    ESP_LOGI(TAG, "Successfully read %d bytes from font file", bytes_read);

    // 打印前32字节的十六进制数据
    ESP_LOGI(TAG, "First 32 bytes (hex):");
    for (int i = 0; i < 32 && i < bytes_read; i++) {
        printf("%02X ", buffer[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }
    if (bytes_read > 0) {
        printf("\n");
    }

    fclose(f);
    ESP_LOGI(TAG, "Font file read test completed successfully");
    return ESP_OK;
}

/**
 * @brief 测试写入helloword.txt文件
 * @return esp_err_t ESP_OK成功，其他错误码失败
 */
esp_err_t test_write_hello_file(void) {
    ESP_LOGI(TAG, "Testing hello file writing...");

    const char* hello_path = "/spiffs/helloword.txt";
    const char* hello_content = "Hello World! This is a test file written to "
                                "SPIFFS.\n你好世界！这是写入SPIFFS的测试文件。\nESP32-S3 SPIFFS Test Demo\n";

    FILE* f = fopen(hello_path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open hello file for writing: %s", hello_path);
        return ESP_FAIL;
    }

    size_t content_len = strlen(hello_content);
    size_t bytes_written = fwrite(hello_content, 1, content_len, f);

    if (bytes_written != content_len) {
        ESP_LOGE(TAG, "Failed to write complete content. Written: %d, Expected: %d", bytes_written, content_len);
        fclose(f);
        return ESP_FAIL;
    }

    fclose(f);
    ESP_LOGI(TAG, "Successfully wrote %d bytes to hello file", bytes_written);
    return ESP_OK;
}

/**
 * @brief 测试读取helloword.txt文件
 * @return esp_err_t ESP_OK成功，其他错误码失败
 */
esp_err_t test_read_hello_file(void) {
    ESP_LOGI(TAG, "Testing hello file reading...");

    const char* hello_path = "/spiffs/helloword.txt";
    FILE* f = fopen(hello_path, "r");

    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open hello file for reading: %s", hello_path);
        return ESP_FAIL;
    }

    // 获取文件大小
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    ESP_LOGI(TAG, "Hello file size: %ld bytes", file_size);

    if (file_size <= 0) {
        ESP_LOGW(TAG, "Hello file is empty");
        fclose(f);
        return ESP_FAIL;
    }

    // 读取文件内容
    char* buffer = malloc(file_size + 1);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for file content");
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    size_t bytes_read = fread(buffer, 1, file_size, f);
    buffer[bytes_read] = '\0'; // 确保字符串以null结尾

    ESP_LOGI(TAG, "Successfully read %d bytes from hello file", bytes_read);
    ESP_LOGI(TAG, "File content:\n%s", buffer);

    free(buffer);
    fclose(f);
    ESP_LOGI(TAG, "Hello file read test completed successfully");
    return ESP_OK;
}

/**
 * @brief 列出SPIFFS分区中的所有文件
 * @return esp_err_t ESP_OK成功，其他错误码失败
 */
esp_err_t list_spiffs_files(void) {
    ESP_LOGI(TAG, "Listing all files in SPIFFS...");

    struct dirent* entry;
    DIR* dir = opendir("/spiffs");

    if (!dir) {
        ESP_LOGE(TAG, "Failed to open SPIFFS directory");
        return ESP_FAIL;
    }

    int file_count = 0;
    while ((entry = readdir(dir)) != NULL) {
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "/spiffs/%s", entry->d_name);

        struct stat file_stat;
        if (stat(filepath, &file_stat) == 0) {
            ESP_LOGI(TAG, "File %d: %s (Size: %ld bytes)", ++file_count, entry->d_name, file_stat.st_size);
        } else {
            ESP_LOGI(TAG, "File %d: %s (Size: unknown)", ++file_count, entry->d_name);
        }
    }

    closedir(dir);
    ESP_LOGI(TAG, "Total files found: %d", file_count);
    return ESP_OK;
}

/**
 * @brief 运行SPIFFS完整测试套件
 * @return esp_err_t ESP_OK成功，其他错误码失败
 */
esp_err_t run_spiffs_test_suite(void) {
    ESP_LOGI(TAG, "=== Starting SPIFFS Test Suite ===");

    esp_err_t ret;

    // 1. 初始化SPIFFS
    ret = spiffs_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS initialization failed");
        return ret;
    }

    // 2. 列出现有文件
    ESP_LOGI(TAG, "\n--- Step 1: List existing files ---");
    list_spiffs_files();

    // 3. 测试读取字体文件
    ESP_LOGI(TAG, "\n--- Step 2: Test reading font file ---");
    ret = test_read_font_file();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Font file read test failed, but continuing with other tests");
    }

    // 4. 测试写入hello文件
    ESP_LOGI(TAG, "\n--- Step 3: Test writing hello file ---");
    ret = test_write_hello_file();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Hello file write test failed");
        return ret;
    }

    // 5. 测试读取hello文件
    ESP_LOGI(TAG, "\n--- Step 4: Test reading hello file ---");
    ret = test_read_hello_file();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Hello file read test failed");
        return ret;
    }

    // 6. 再次列出文件，确认新文件已创建
    ESP_LOGI(TAG, "\n--- Step 5: List files after write test ---");
    list_spiffs_files();

    // 7. 显示SPIFFS使用情况
    size_t total = 0, used = 0;
    esp_spiffs_info("spiffs", &total, &used);
    ESP_LOGI(TAG, "\n--- SPIFFS Usage Summary ---");
    ESP_LOGI(TAG, "Total: %d bytes, Used: %d bytes, Free: %d bytes", total, used, total - used);
    ESP_LOGI(TAG, "Usage: %.1f%%", (float)used / total * 100);

    ESP_LOGI(TAG, "=== SPIFFS Test Suite Completed Successfully ===");
    return ESP_OK;
}

/**
 * @brief 卸载SPIFFS文件系统
 */
void spiffs_deinit(void) {
    esp_vfs_spiffs_unregister("spiffs");
    ESP_LOGI(TAG, "SPIFFS unmounted");
}
