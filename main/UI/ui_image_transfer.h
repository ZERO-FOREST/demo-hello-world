#ifndef UI_IMAGE_TRANSFER_H
#define UI_IMAGE_TRANSFER_H

#include "lvgl.h"
#include "esp_jpeg_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Creates the image transfer UI screen.
 * @param parent The parent object for the screen.
 */
void ui_image_transfer_create(lv_obj_t *parent);

/**
 * @brief Updates the image on the image transfer UI screen.
 * @param img_buf Pointer to the decoded image buffer.
 * @param width Width of the decoded image.
 * @param height Height of the decoded image.
 * @param format Pixel format of the decoded image.
 */
void ui_image_transfer_set_image_data(uint8_t *img_buf, int width, int height, jpeg_pixel_format_t format);

#ifdef __cplusplus
}
#endif

#endif // UI_IMAGE_TRANSFER_H