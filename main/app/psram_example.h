#ifndef PSRAM_EXAMPLE_H
#define PSRAM_EXAMPLE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 演示PSRAM使用方法的示例函数
 */
void psram_usage_examples(void);

/**
 * @brief 为图像处理分配PSRAM缓冲区
 * @param width 图像宽度
 * @param height 图像高度  
 * @param bytes_per_pixel 每像素字节数
 * @return 分配的缓冲区指针，失败返回NULL
 */
void* allocate_image_buffer(size_t width, size_t height, size_t bytes_per_pixel);

/**
 * @brief 释放图像缓冲区
 * @param buffer 要释放的缓冲区指针
 */
void free_image_buffer(void* buffer);

#ifdef __cplusplus
}
#endif

#endif // PSRAM_EXAMPLE_H
