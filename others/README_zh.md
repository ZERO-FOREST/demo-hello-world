以下是 README.md 的中文翻译：

# ESP_NEW_JPEG

ESP_NEW_JPEG 是乐鑫推出的轻量级 JPEG 编码和解码库。其内存和 CPU 占用经过优化，更好地适配乐鑫芯片。

## 特性

### 编码器

- 支持多种宽高的编码
- 支持 RGB888、RGBA、YCbYCr、YCbY2YCrY2、GRAY 像素格式
- 支持 YUV444、YUV422、YUV420 子采样
- 支持质量（1-100）
- 支持 0、90、180、270 度顺时针旋转，适用于以下情况：
  1. src_type = JPEG_PIXEL_FORMAT_YCbYCr，subsampling = JPEG_SUBSAMPLE_420，且宽高为 16 的倍数
  2. src_type = JPEG_PIXEL_FORMAT_YCbYCr，subsampling = JPEG_SUBSAMPLE_GRAY，且宽高为 8 的倍数
- 支持单任务和双任务
- 支持两种编码模式：单图像编码和块编码

### 解码器

- 支持多种宽高的解码
- 支持单通道和三通道解码
- 支持 RGB888、RGB565（大端）、RGB565（小端）、CbYCrY 像素格式输出
- 支持 0、90、180、270 度顺时针旋转，宽高为 8 的倍数时适用
- 支持裁剪和缩放，宽高为 8 的倍数时适用
- 支持两种解码模式：单图像解码和块解码

## 性能

### ESP32-S3 芯片测试

#### 编码器

单任务编码器消耗内存（10 kByte DRAM）为常数。

| 分辨率      | 源像素格式                | 输出质量 | 输出子采样         | 帧率（fps） |
|-------------|--------------------------|----------|--------------------|-------------|
| 1920 * 1080 | JPEG_PIXEL_FORMAT_YCbYCr | 60       | JPEG_SUBSAMPLE_420 | 1.59        |
| 1920 * 1080 | JPEG_PIXEL_FORMAT_RGB888 | 60       | JPEG_SUBSAMPLE_420 | 1.33        |
| 1280 * 720  | JPEG_PIXEL_FORMAT_YCbYCr | 60       | JPEG_SUBSAMPLE_420 | 4.84        |
| 1280 * 720  | JPEG_PIXEL_FORMAT_RGB888 | 60       | JPEG_SUBSAMPLE_420 | 2.92        |
| 800 * 480   | JPEG_PIXEL_FORMAT_YCbYCr | 60       | JPEG_SUBSAMPLE_420 | 10.82       |
| 800 * 480   | JPEG_PIXEL_FORMAT_RGB888 | 60       | JPEG_SUBSAMPLE_420 | 6.74        |
| 640 * 480   | JPEG_PIXEL_FORMAT_YCbYCr | 60       | JPEG_SUBSAMPLE_420 | 13.24       |
| 640 * 480   | JPEG_PIXEL_FORMAT_RGB888 | 60       | JPEG_SUBSAMPLE_420 | 8.32        |
| 480 * 320   | JPEG_PIXEL_FORMAT_YCbYCr | 60       | JPEG_SUBSAMPLE_420 | 24.35       |
| 480 * 320   | JPEG_PIXEL_FORMAT_RGB888 | 60       | JPEG_SUBSAMPLE_420 | 15.84       |
| 320 * 240   | JPEG_PIXEL_FORMAT_YCbYCr | 60       | JPEG_SUBSAMPLE_420 | 45.30       |
| 320 * 240   | JPEG_PIXEL_FORMAT_RGB888 | 60       | JPEG_SUBSAMPLE_420 | 30.37       |

双任务编码器开启时，图像越宽，内存消耗越大。

| 分辨率     | 源像素格式                | 输出质量 | 输出子采样         | 帧率（fps） |
|------------|--------------------------|----------|--------------------|-------------|
| 1280 * 720 | JPEG_PIXEL_FORMAT_RGB888 | 60       | JPEG_SUBSAMPLE_420 | 4.62        |
| 800 * 480  | JPEG_PIXEL_FORMAT_RGB888 | 60       | JPEG_SUBSAMPLE_420 | 10.46       |
| 640 * 480  | JPEG_PIXEL_FORMAT_RGB888 | 60       | JPEG_SUBSAMPLE_420 | 12.89       |
| 480 * 320  | JPEG_PIXEL_FORMAT_RGB888 | 60       | JPEG_SUBSAMPLE_420 | 23.57       |
| 320 * 240  | JPEG_PIXEL_FORMAT_RGB888 | 60       | JPEG_SUBSAMPLE_420 | 43.97       |

#### 解码器

消耗内存（10 kByte DRAM）为常数。

旋转 JPEG_ROTATE_0D 情况：

| 分辨率      | 源子采样         | 源质量 | 输出像素格式                | 帧率（fps） |
|-------------|------------------|--------|-----------------------------|-------------|
| 1920 * 1080 | JPEG_SUBSAMPLE_422 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 3.27        |
| 1920 * 1080 | JPEG_SUBSAMPLE_420 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 3.78        |
| 1280 * 720  | JPEG_SUBSAMPLE_422 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 6.77        |
| 1280 * 720  | JPEG_SUBSAMPLE_420 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 7.82        |
| 800 * 480   | JPEG_SUBSAMPLE_422 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 14.73       |
| 800 * 480   | JPEG_SUBSAMPLE_420 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 16.87       |
| 640 * 480   | JPEG_SUBSAMPLE_422 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 17.90       |
| 640 * 480   | JPEG_SUBSAMPLE_420 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 20.46       |
| 480 * 320   | JPEG_SUBSAMPLE_422 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 32.27       |
| 480 * 320   | JPEG_SUBSAMPLE_420 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 36.29       |
| 320 * 240   | JPEG_SUBSAMPLE_422 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 58.95       |
| 320 * 240   | JPEG_SUBSAMPLE_420 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 66.28       |

旋转 JPEG_ROTATE_90D 情况：

| 分辨率      | 源子采样         | 源质量 | 输出像素格式                | 帧率（fps） |
|-------------|------------------|--------|-----------------------------|-------------|
| 1920 * 1080 | JPEG_SUBSAMPLE_422 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 2.23        |
| 1920 * 1080 | JPEG_SUBSAMPLE_420 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 3.11        |
| 1280 * 720  | JPEG_SUBSAMPLE_422 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 5.02        |
| 1280 * 720  | JPEG_SUBSAMPLE_420 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 7.13        |
| 800 * 480   | JPEG_SUBSAMPLE_422 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 14.09       |
| 800 * 480   | JPEG_SUBSAMPLE_420 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 16.85       |
| 640 * 480   | JPEG_SUBSAMPLE_422 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 17.16       |
| 640 * 480   | JPEG_SUBSAMPLE_420 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 20.45       |
| 480 * 320   | JPEG_SUBSAMPLE_422 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 30.87       |
| 480 * 320   | JPEG_SUBSAMPLE_420 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 36.15       |
| 320 * 240   | JPEG_SUBSAMPLE_422 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 59.17       |
| 320 * 240   | JPEG_SUBSAMPLE_420 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 70.78       |

### ESP32-S2 芯片测试

#### 编码器

仅支持单任务。消耗内存（10 kByte DRAM）为常数。

| 分辨率     | 源像素格式                | 输出质量 | 输出子采样         | 帧率（fps） |
|------------|--------------------------|----------|--------------------|-------------|
| 800 * 480  | JPEG_PIXEL_FORMAT_YCbYCr | 60       | JPEG_SUBSAMPLE_420 | 3.60        |
| 800 * 480  | JPEG_PIXEL_FORMAT_RGB888 | 60       | JPEG_SUBSAMPLE_420 | 2.76        |
| 640 * 480  | JPEG_PIXEL_FORMAT_YCbYCr | 60       | JPEG_SUBSAMPLE_420 | 4.47        |
| 640 * 480  | JPEG_PIXEL_FORMAT_RGB888 | 60       | JPEG_SUBSAMPLE_420 | 3.43        |
| 480 * 320  | JPEG_PIXEL_FORMAT_YCbYCr | 60       | JPEG_SUBSAMPLE_420 | 8.64        |
| 480 * 320  | JPEG_PIXEL_FORMAT_RGB888 | 60       | JPEG_SUBSAMPLE_420 | 6.69        |
| 320 * 240  | JPEG_PIXEL_FORMAT_YCbYCr | 60       | JPEG_SUBSAMPLE_420 | 16.30       |
| 320 * 240  | JPEG_PIXEL_FORMAT_RGB888 | 60       | JPEG_SUBSAMPLE_420 | 13.05       |

#### 解码器

消耗内存（10 kByte DRAM）为常数。

旋转 JPEG_ROTATE_0D 情况：

| 分辨率     | 源子采样         | 源质量 | 输出像素格式                | 帧率（fps） |
|------------|------------------|--------|-----------------------------|-------------|
| 800 * 480  | JPEG_SUBSAMPLE_422 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 5.44        |
| 800 * 480  | JPEG_SUBSAMPLE_420 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 5.76        |
| 640 * 480  | JPEG_SUBSAMPLE_422 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 6.70        |
| 640 * 480  | JPEG_SUBSAMPLE_420 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 7.09        |
| 480 * 320  | JPEG_SUBSAMPLE_422 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 12.71       |
| 480 * 320  | JPEG_SUBSAMPLE_420 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 13.42       |
| 320 * 240  | JPEG_SUBSAMPLE_422 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 24.18       |
| 320 * 240  | JPEG_SUBSAMPLE_420 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 25.60       |

旋转 JPEG_ROTATE_90D 情况：

| 分辨率     | 源子采样         | 源质量 | 输出像素格式                | 帧率（fps） |
|------------|------------------|--------|-----------------------------|-------------|
| 800 * 480  | JPEG_SUBSAMPLE_422 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 4.53        |
| 800 * 480  | JPEG_SUBSAMPLE_420 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 4.81        |
| 640 * 480  | JPEG_SUBSAMPLE_422 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 5.59        |
| 640 * 480  | JPEG_SUBSAMPLE_420 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 5.94        |
| 480 * 320  | JPEG_SUBSAMPLE_422 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 10.69       |
| 480 * 320  | JPEG_SUBSAMPLE_420 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 11.33       |
| 320 * 240  | JPEG_SUBSAMPLE_422 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 22.40       |
| 320 * 240  | JPEG_SUBSAMPLE_420 | 60   | JPEG_PIXEL_FORMAT_RGB565_LE | 21.77       |

## 用法

请参考 `test_app` 文件夹获取更多 API 使用细节。

- `test_app/main/test_encoder.c` 编码器相关
  - 编码单张图片
  - 使用块编码 API 编码单张图片
- `test_app/main/test_decoder.c` 解码器相关
  - 解码单张 JPEG 图片
  - 使用块解码 API 解码单张 JPEG 图片
  - 解码同尺寸 JPEG 流

## 常见问题

1. ESP_NEW_JPEG 支持解码渐进式 JPEG 吗？

   不支持，仅支持解码基线 JPEG。

   可用如下代码判断图片是否为渐进式 JPEG。输出 1 表示渐进式，0 表示基线：

    ```bash
    python
    >>> from PIL import Image
    >>> Image.open("file_name.jpg").info.get('progressive', 0)
    ```

2. 为什么输出图片出现错位？

   通常是图片最左或最右几列像素出现在了另一侧。如果你使用的是 ESP32-S3，可能原因是解码器输出缓冲区或编码器输入缓冲区未按 16 字节对齐。请使用 `jpeg_calloc_align` 分配缓冲区。

3. 如何简单验证编码器输出是否正确？

   可用如下代码直接打印输出 JPEG 数据。复制输出数据，粘贴到十六进制编辑器，保存为 .jpg 文件即可。

    ```c
    for (int i = 0; i < out_len; i++) {
        printf("%02x", outbuf[i]);
    }
    printf("\n");
    ```

## 支持芯片

下表展示了 ESP_NEW_JPEG 对乐鑫 SoC 的支持情况。“✔”表示支持，“✘”表示不支持。

| 芯片      | v0.6.1   |
|-----------|----------|
| ESP32     | ✔        |
| ESP32-S2  | ✔        |
| ESP32-S3  | ✔        |
| ESP32-P4  | ✔        |
| ESP32-C2  | ✔        |
| ESP32-C3  | ✔        |
| ESP32-C5  | ✔        |
| ESP32-C6  | ✔        |

如需进一步细化或格式调整，请告知。