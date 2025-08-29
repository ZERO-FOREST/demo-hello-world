#ifndef JPEG_STREAM_ENCODER_H
#define JPEG_STREAM_ENCODER_H

#include "esp_err.h"
#include "esp_jpeg_common.h"
#include "esp_jpeg_enc.h"


#ifdef __cplusplus
extern "C" {
#endif

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

typedef enum {
    JPEG_STREAM_ID_USB = 0,
    JPEG_STREAM_ID_SPI = 1,
} jpeg_stream_id_t;

typedef struct jpeg_stream_handle_s* jpeg_stream_handle_t;

typedef struct {
    int width;
    int height;
    jpeg_pixel_format_t src_type;
    jpeg_subsampling_t subsampling;
    uint8_t quality;
    jpeg_stream_id_t stream_id;
} jpeg_stream_config_t;

esp_err_t jpeg_stream_create(const jpeg_stream_config_t* cfg, jpeg_stream_handle_t* out);
void jpeg_stream_destroy(jpeg_stream_handle_t h);
esp_err_t jpeg_stream_feed(jpeg_stream_handle_t h, const uint8_t* data, size_t len);

// 弱符号回调：用户可在别处实现覆盖，处理编码后的 JPEG 数据
__attribute__((weak)) void on_jpeg_frame_encoded(const uint8_t* jpg, int jpg_size, const jpeg_stream_config_t* cfg);

#ifdef __cplusplus
}
#endif

#endif // JPEG_STREAM_ENCODER_H
