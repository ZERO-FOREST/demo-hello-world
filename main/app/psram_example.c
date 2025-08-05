#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"

static const char* TAG = "PSRAM_EXAMPLE";

void psram_usage_examples(void)
{
    ESP_LOGI(TAG, "=== PSRAM Usage Examples ===");
    
    // 检查PSRAM是否可用
    size_t psram_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    
    ESP_LOGI(TAG, "PSRAM Total: %u bytes", psram_size);
    ESP_LOGI(TAG, "PSRAM Free: %u bytes", psram_free);
    
    if (psram_size == 0) {
        ESP_LOGW(TAG, "PSRAM not available!");
        return;
    }
    
    // 方式1：显式从PSRAM分配内存
    size_t buffer_size = 100 * 1024; // 100KB
    void* psram_buffer = heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
    
    if (psram_buffer != NULL) {
        ESP_LOGI(TAG, "Successfully allocated %u bytes from PSRAM at %p", 
                 buffer_size, psram_buffer);
        
        // 使用内存（比如存储图像数据）
        memset(psram_buffer, 0xAA, buffer_size);
        
        // 释放内存
        heap_caps_free(psram_buffer);
        ESP_LOGI(TAG, "PSRAM buffer freed");
    } else {
        ESP_LOGE(TAG, "Failed to allocate PSRAM buffer");
    }
    
    // 方式2：分配内部RAM（对比）
    void* internal_buffer = heap_caps_malloc(4096, MALLOC_CAP_INTERNAL);
    if (internal_buffer != NULL) {
        ESP_LOGI(TAG, "Internal RAM buffer allocated at %p", internal_buffer);
        heap_caps_free(internal_buffer);
    }
    
    // 方式3：如果启用了CONFIG_SPIRAM_USE_MALLOC，普通malloc也会使用PSRAM
    void* normal_malloc_buffer = malloc(50 * 1024); // 50KB
    if (normal_malloc_buffer != NULL) {
        ESP_LOGI(TAG, "Normal malloc buffer allocated at %p", normal_malloc_buffer);
        
        // 检查这个指针是否在PSRAM中
        if (heap_caps_check_add_region_allowed((intptr_t)normal_malloc_buffer, 
                                               50 * 1024, MALLOC_CAP_SPIRAM)) {
            ESP_LOGI(TAG, "This buffer is likely in PSRAM");
        }
        
        free(normal_malloc_buffer);
    }
    
    // 方式4：为LVGL分配PSRAM内存（如果需要）
    void* lvgl_buffer = heap_caps_malloc(320 * 240 * 2, MALLOC_CAP_SPIRAM); // 显示缓冲区
    if (lvgl_buffer != NULL) {
        ESP_LOGI(TAG, "LVGL display buffer allocated in PSRAM at %p", lvgl_buffer);
        heap_caps_free(lvgl_buffer);
    }
    
    // 显示内存使用情况
    ESP_LOGI(TAG, "=== Memory Status After Operations ===");
    ESP_LOGI(TAG, "PSRAM Free: %u bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "Internal RAM Free: %u bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI(TAG, "Total Free: %u bytes", heap_caps_get_free_size(MALLOC_CAP_8BIT));
}

// 为图像处理分配PSRAM缓冲区的实用函数
void* allocate_image_buffer(size_t width, size_t height, size_t bytes_per_pixel)
{
    size_t buffer_size = width * height * bytes_per_pixel;
    void* buffer = heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
    
    if (buffer != NULL) {
        ESP_LOGI(TAG, "Image buffer (%ux%u, %u bpp) allocated: %u bytes at %p", 
                 width, height, bytes_per_pixel, buffer_size, buffer);
    } else {
        ESP_LOGE(TAG, "Failed to allocate image buffer: %u bytes", buffer_size);
    }
    
    return buffer;
}

void free_image_buffer(void* buffer)
{
    if (buffer != NULL) {
        heap_caps_free(buffer);
        ESP_LOGI(TAG, "Image buffer freed");
    }
}
