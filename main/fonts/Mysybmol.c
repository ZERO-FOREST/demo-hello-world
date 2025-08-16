/*******************************************************************************
 * Size: 16 px
 * Bpp: 2
 * Opts: --bpp 2 --size 16 --no-compress --stride 1 --align 1 --font iconfont.ttf --range 58930,58890,58910,59559,58883
 *--format lvgl -o Mysybmol.c
 ******************************************************************************/

#ifdef __has_include
#if __has_include("lvgl.h")
#ifndef LV_LVGL_H_INCLUDE_SIMPLE
#define LV_LVGL_H_INCLUDE_SIMPLE
#endif
#endif
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef MYSYBMOL
#define MYSYBMOL 1
#endif

#if MYSYBMOL

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+E603 "" EE98B2 */
    0x2f, 0xff, 0xff, 0xf8, 0x30, 0x0, 0x0, 0xc, 0x30, 0x4, 0x0, 0xc, 0x30, 0xf, 0x80, 0xc, 0x30, 0xc, 0x74, 0xc, 0x30,
    0xc, 0x2c, 0xc, 0x30, 0xe, 0xd0, 0xc, 0x30, 0xe, 0x0, 0xc, 0x30, 0x0, 0x0, 0xc, 0x31, 0xff, 0xff, 0x4c, 0x30, 0x0,
    0x0, 0xc, 0x2f, 0xff, 0xff, 0xf8,

    /* U+E60A "" EE988A */
    0x0, 0x0, 0x1, 0x80, 0x0, 0xb, 0x47, 0xc0, 0x0, 0x3, 0xef, 0x0, 0x0, 0x1d, 0xfc, 0xd, 0x0, 0x7f, 0x7d, 0x3c, 0x1,
    0xff, 0xdf, 0xf4, 0x2, 0xff, 0xf7, 0xe0, 0x3, 0xff, 0xfd, 0xf4, 0x3, 0xff, 0xff, 0x7c, 0x2, 0xff, 0xff, 0xc4, 0xe,
    0xbf, 0xff, 0x40, 0x7, 0xaf, 0xfd, 0x0, 0x1d, 0xeb, 0xe0, 0x0, 0x1f, 0x60, 0x0, 0x0, 0x75, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0,

    /* U+E61E "" EE989E */
    0x4, 0x0, 0x0, 0x3, 0xf4, 0x30, 0x20, 0x31, 0xc3, 0x2, 0x3, 0x68, 0x30, 0x20, 0xe, 0x3, 0x2, 0x0, 0x90, 0xf4, 0x20,
    0x9, 0x34, 0xc2, 0x0, 0x93, 0x4c, 0x20, 0x9, 0xf, 0x42, 0x0, 0x90, 0x30, 0xb4, 0x9, 0x3, 0x29, 0xc0, 0x90, 0x32,
    0x4c, 0x9, 0x3, 0x1f, 0xc0, 0x0, 0x0, 0x10,

    /* U+E632 "" EE98B2*/
    0x0, 0x0, 0x0, 0x0, 0x0, 0x6f, 0xfd, 0x0, 0x3, 0x90, 0x6, 0xc0, 0xd, 0xa, 0xa0, 0x70, 0x30, 0x75, 0x5e, 0xc, 0x1,
    0xc0, 0x2, 0x80, 0x3, 0x7, 0xd0, 0x80, 0x0, 0x28, 0x28, 0x0, 0x0, 0x20, 0x8, 0x0, 0x0, 0x4, 0x10, 0x0, 0x0, 0x7,
    0xd0, 0x0, 0x0, 0x3, 0xc0, 0x0, 0x0, 0x9, 0x70, 0x0, 0x0, 0x0, 0x0, 0x0,

    /* U+E8A7 "" EE9A77*/
    0x15, 0x55, 0x25, 0x55, 0x79, 0xea, 0xaa, 0x50, 0xa, 0x94, 0x2, 0xa5, 0x0, 0xa9, 0xaa, 0xaa, 0x0, 0x2, 0x80, 0x0,
    0xa0, 0x82, 0xa8, 0xb8, 0xa, 0x8, 0x32, 0x80, 0x0, 0xaa, 0xaa, 0x80};

/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 256, .box_w = 16, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 48, .adv_w = 256, .box_w = 16, .box_h = 16, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 112, .adv_w = 256, .box_w = 14, .box_h = 14, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 161, .adv_w = 256, .box_w = 16, .box_h = 14, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 217, .adv_w = 256, .box_w = 9, .box_h = 14, .ofs_x = 3, .ofs_y = 0}};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {0x0, 0x7, 0x1b, 0x2f, 0x2a4};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] = {{.range_start = 58883,
                                                .range_length = 677,
                                                .glyph_id_start = 1,
                                                .unicode_list = unicode_list_0,
                                                .glyph_id_ofs_list = NULL,
                                                .list_length = 5,
                                                .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY}};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 2,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif

};

/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
const lv_font_t Mysybmol = {
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt, /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt, /*Function pointer to get glyph's bitmap*/
    .line_height = 16,                              /*The maximum line height required by the font*/
    .base_line = 2,                                 /*Baseline measured from the bottom of the line*/
    .underline_position = 0,
    .underline_thickness = 0,
    .dsc = &font_dsc, /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
    .user_data = NULL,
};

#endif /*#if MYSYBMOL*/
