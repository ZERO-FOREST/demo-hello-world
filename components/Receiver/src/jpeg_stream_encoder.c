#include "jpeg_stream_encoder.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>


typedef struct jpeg_stream_handle_s {
    jpeg_enc_handle_t enc;
    jpeg_stream_config_t cfg;
    int frame_bytes_expected;
    int frame_bytes_accum;
    uint8_t* frame_buf; // 对齐输入帧缓冲
    uint8_t* jpg_buf;   // 输出 JPEG 缓冲
    int jpg_buf_size;
} jpeg_stream_handle_impl_t;

static const char* TAG = "jpeg_enc";

esp_err_t jpeg_stream_create(const jpeg_stream_config_t* user_cfg, jpeg_stream_handle_t* out) {
    if (!user_cfg || !out)
        return ESP_ERR_INVALID_ARG;

    jpeg_stream_handle_impl_t* h = (jpeg_stream_handle_impl_t*)calloc(1, sizeof(*h));
    if (!h)
        return ESP_ERR_NO_MEM;

    h->cfg = *user_cfg;

    jpeg_enc_config_t enc_cfg = DEFAULT_JPEG_ENC_CONFIG();
    enc_cfg.width = h->cfg.width;
    enc_cfg.height = h->cfg.height;
    enc_cfg.src_type = h->cfg.src_type;
    enc_cfg.subsampling = h->cfg.subsampling;
    enc_cfg.quality = h->cfg.quality;
    enc_cfg.task_enable = true;
#ifdef ESP_PLATFORM
    enc_cfg.hfm_task_priority = 13;
    enc_cfg.hfm_task_core = 1;
#endif

    if (jpeg_enc_open(&enc_cfg, &h->enc) != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "jpeg_enc_open failed");
        free(h);
        return ESP_FAIL;
    }

    // 计算一帧原始数据大小
    float bpp = 0.0f;
    switch (h->cfg.src_type) {
    case JPEG_PIXEL_FORMAT_YCbYCr:
        bpp = 2.0f;
        break; // YUYV 2 bytes per pixel
    case JPEG_PIXEL_FORMAT_RGB888:
        bpp = 3.0f;
        break;
    case JPEG_PIXEL_FORMAT_RGBA:
        bpp = 4.0f;
        break;
    case JPEG_PIXEL_FORMAT_GRAY:
        bpp = 1.0f;
        break;
    case JPEG_PIXEL_FORMAT_YCbY2YCrY2:
        bpp = 1.5f;
        break; // 12 bytes per 8 pixels
    default:
        bpp = 2.0f;
        break;
    }
    h->frame_bytes_expected = (int)(h->cfg.width * h->cfg.height * bpp);

    // 输入帧需 16 字节对齐
    h->frame_buf = (uint8_t*)jpeg_calloc_align(h->frame_bytes_expected, 16);
    if (!h->frame_buf) {
        jpeg_enc_close(h->enc);
        free(h);
        return ESP_ERR_NO_MEM;
    }

    // 粗略分配 JPEG 输出缓冲：原始的 1/3 作为上限（根据质量可调）
    h->jpg_buf_size = h->frame_bytes_expected / 3 + 1024;
    h->jpg_buf = (uint8_t*)malloc(h->jpg_buf_size);
    if (!h->jpg_buf) {
        jpeg_free_align(h->frame_buf);
        jpeg_enc_close(h->enc);
        free(h);
        return ESP_ERR_NO_MEM;
    }

    *out = (jpeg_stream_handle_t)h;
    return ESP_OK;
}

void jpeg_stream_destroy(jpeg_stream_handle_t handle) {
    if (!handle)
        return;
    jpeg_stream_handle_impl_t* h = (jpeg_stream_handle_impl_t*)handle;
    if (h->enc)
        jpeg_enc_close(h->enc);
    if (h->frame_buf)
        jpeg_free_align(h->frame_buf);
    if (h->jpg_buf)
        free(h->jpg_buf);
    free(h);
}

esp_err_t jpeg_stream_feed(jpeg_stream_handle_t handle, const uint8_t* data, size_t len) {
    if (!handle || !data || len == 0)
        return ESP_ERR_INVALID_ARG;
    jpeg_stream_handle_impl_t* h = (jpeg_stream_handle_impl_t*)handle;

    size_t to_copy = len;
    if (h->frame_bytes_accum + (int)to_copy > h->frame_bytes_expected) {
        to_copy = h->frame_bytes_expected - h->frame_bytes_accum;
    }
    memcpy(&h->frame_buf[h->frame_bytes_accum], data, to_copy);
    h->frame_bytes_accum += (int)to_copy;

    // 若一帧已满，执行编码
    if (h->frame_bytes_accum >= h->frame_bytes_expected) {
        int out_size = 0;
        jpeg_error_t err =
            jpeg_enc_process(h->enc, h->frame_buf, h->frame_bytes_expected, h->jpg_buf, h->jpg_buf_size, &out_size);
        if (err == JPEG_ERR_OK && out_size > 0) {
            on_jpeg_frame_encoded(h->jpg_buf, out_size, &h->cfg);
        } else {
            ESP_LOGE(TAG, "jpeg_enc_process err=%d", err);
        }

        // 重置累计，准备下一帧
        h->frame_bytes_accum = 0;
    }

    // 若还有未消费的数据（超出一帧），递归/循环继续喂入
    size_t remain = len - to_copy;
    if (remain > 0) {
        return jpeg_stream_feed(handle, &data[to_copy], remain);
    }

    return ESP_OK;
}

__attribute__((weak)) void on_jpeg_frame_encoded(const uint8_t* jpg, int jpg_size, const jpeg_stream_config_t* cfg) {
    // 默认空实现，用户可在其他文件重写此回调：如保存到 SPIFFS 或通过网络回传
    (void)jpg;
    (void)jpg_size;
    (void)cfg;
}
