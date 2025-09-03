#ifndef JPEG_STREAM_ENCODER_H
#define JPEG_STREAM_ENCODER_H

#include "esp_err.h"
#include "esp_jpeg_common.h"
#include "esp_jpeg_enc.h"


// 可根据输入源定制默认分辨率/像素格式/质量
#ifndef JPEG_STREAM_WIDTH
#define JPEG_STREAM_WIDTH 240
#endif
#ifndef JPEG_STREAM_HEIGHT
#define JPEG_STREAM_HEIGHT 320
#endif
#ifndef JPEG_STREAM_SRC_FMT
#define JPEG_STREAM_SRC_FMT JPEG_PIXEL_FORMAT_YCbYCr // 常见 YUV422 打包（YUYV）每像素2字节
#endif

#ifndef JPEG_STREAM_SUBSAMPLE
#define JPEG_STREAM_SUBSAMPLE JPEG_SUBSAMPLE_420
#endif
#ifndef JPEG_STREAM_QUALITY
#define JPEG_STREAM_QUALITY 70
#endif

#endif // JPEG_STREAM_ENCODER_H
