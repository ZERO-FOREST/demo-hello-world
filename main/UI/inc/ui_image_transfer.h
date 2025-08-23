#ifndef UI_IMAGE_TRANSFER_H
#define UI_IMAGE_TRANSFER_H

#include "lvgl.h"
#include "p2p_udp_image_transfer.h" // Note: This will be changed later
#include "wifi_image_transfer.h"

// Function Prototypes
void ui_image_transfer_create(lv_obj_t* parent);
void ui_image_transfer_destroy(void);
void ui_image_transfer_set_image_data(uint8_t* img_buf, int width, int height, jpeg_pixel_format_t format);

#endif // UI_IMAGE_TRANSFER_H
