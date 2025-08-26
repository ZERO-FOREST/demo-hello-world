#include "my_font.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_spi_flash.h"
#include "lvgl.h"
#include <stdbool.h>
#include <string.h>

static const char *TAG = "FONT_INIT";
lv_font_t *font_cn = NULL;

typedef struct {
    const uint8_t *data_ptr;
    size_t size;
    size_t current_pos;
} mmap_file_t;

static const void *mmap_ptr = NULL;
static size_t font_partition_size = 0;

static void* mmap_open(lv_fs_drv_t* drv, const char* path, lv_fs_mode_t mode) {
    if (mode != LV_FS_MODE_RD || mmap_ptr == NULL) {
        return NULL;
    }
    
    mmap_file_t *file = lv_mem_alloc(sizeof(mmap_file_t));
    if (file == NULL) return NULL;

    file->data_ptr = mmap_ptr;
    file->size = font_partition_size;
    file->current_pos = 0;
    
    return file;
}

static lv_fs_res_t mmap_close(lv_fs_drv_t* drv, void* file_p) {
    lv_mem_free(file_p);
    return LV_FS_RES_OK;
}

static lv_fs_res_t mmap_read(lv_fs_drv_t* drv, void* file_p, void* buf, uint32_t btr, uint32_t* br) {
    mmap_file_t *file = (mmap_file_t*)file_p;
    
    size_t bytes_to_read = btr;
    if (file->current_pos + bytes_to_read > file->size) {
        bytes_to_read = file->size - file->current_pos;
    }
    
    memcpy(buf, file->data_ptr + file->current_pos, bytes_to_read);
    file->current_pos += bytes_to_read;
    *br = bytes_to_read;
    
    return LV_FS_RES_OK;
}

static lv_fs_res_t mmap_seek(lv_fs_drv_t* drv, void* file_p, uint32_t pos, lv_fs_whence_t whence) {
    mmap_file_t *file = (mmap_file_t*)file_p;

    switch (whence) {
        case LV_FS_SEEK_SET:
            file->current_pos = pos;
            break;
        case LV_FS_SEEK_CUR:
            file->current_pos += pos;
            break;
        case LV_FS_SEEK_END:
            file->current_pos = file->size + pos;
            break;
    }

    if (file->current_pos > file->size) {
        file->current_pos = file->size;
    }

    return LV_FS_RES_OK;
}

static lv_fs_res_t mmap_tell(lv_fs_drv_t* drv, void* file_p, uint32_t* pos_p) {
    mmap_file_t *file = (mmap_file_t*)file_p;
    *pos_p = file->current_pos;
    return LV_FS_RES_OK;
}

void font_init(void) {
    
    const esp_partition_t *font_partition = esp_partition_find_first(0x40, 0, "font");
    if (font_partition == NULL) {
        ESP_LOGE(TAG, "Font partition 'font' not found!");
        return;
    }
    
    font_partition_size = font_partition->size;
    spi_flash_mmap_handle_t mmap_handle;
    esp_err_t err = esp_partition_mmap(font_partition, 0, font_partition->size, SPI_FLASH_MMAP_DATA, &mmap_ptr, &mmap_handle);
    if (err != ESP_OK || mmap_ptr == NULL) {
        ESP_LOGE(TAG, "Failed to mmap font partition: %s", esp_err_to_name(err));
        return;
    }

    static lv_fs_drv_t drv;
    lv_fs_drv_init(&drv);

    drv.letter = 'P';
    drv.open_cb = mmap_open;
    drv.close_cb = mmap_close;
    drv.read_cb = mmap_read;
    drv.seek_cb = mmap_seek;
    drv.tell_cb = mmap_tell;
    lv_fs_drv_register(&drv);
    
    font_cn = lv_font_load("P:font.bin");
    
    if (font_cn) {
        ESP_LOGI(TAG, "Font loaded successfully from partition!");
    } else {
        ESP_LOGE(TAG, "Failed to load font from partition.");
    }
}

bool is_font_loaded(void) {
    return font_cn != NULL;
}

lv_font_t *get_loaded_font(void) {
    return font_cn;
}
