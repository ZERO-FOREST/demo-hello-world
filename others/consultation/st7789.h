#ifndef __ST7789_H
#define __ST7789_H

#include "main.h"
#include "cmsis_os.h"
#include "fonts.h"
#include "tim.h"
#include "spi.h"
#include "stdbool.h"

/* choose a Hardware SPI port to use. */
#define ST7789_SPI_PORT hspi1
extern SPI_HandleTypeDef ST7789_SPI_PORT;
/* choose a Hardware PWM to use.*/
#define ST7789_PWM_TIM htim10
extern TIM_HandleTypeDef ST7789_PWM_TIM;
#define ST7789_PWM_CH TIM_CHANNEL_1
// #define CHxN	//Enable if using a complementary PWM (CHxN)

/* choose whether use DMA or not */
#define USE_DMA

/* If you want to drive backlight choose if you want PWM or Binary(ON/OFF) control */
#define BL_PWM // backlite BL_PWM or BL_BIN

/* If u need CS control, comment below*/
// #define CFG_NO_CS
/* Pin connection*/
#define ST7789_RST_PORT SPI_INT_GPIO_Port
#define ST7789_RST_PIN SPI_INT_Pin
#define ST7789_DC_PORT IPS_DC_GPIO_Port
#define ST7789_DC_PIN IPS_DC_Pin

#ifndef CFG_NO_CS
#define ST7789_CS_PORT IPS_CS_GPIO_Port
#define ST7789_CS_PIN IPS_CS_Pin
#endif

#define WINVERT(data) ((data >> 8) | (data << 8)) // Invert a color (word invert)

/* If u need Backlight control, uncomment below */
#define ST7789_BLK_PORT IPS_BLK_GPIO_Port
#define ST7789_BLK_PIN IPS_BLK_Pin

/*
 * Comment one to use another.
 * 3 parameters can be choosed
 * 135x240(0.96 inch) & 240x240(1.3inch) & 170x320(1.9inch)
 * X_SHIFT & Y_SHIFT are used to adapt different display's resolution
 */

/* Choose a type you are using */
// #define USING_135X240
// #define USING_240X240
#define USING_240X282
// #define USING_170X320

/* Choose a display rotation you want to use: (0-3) */
#define ST7789_ROTATION 2

#ifdef USING_135X240

#if ST7789_ROTATION == 0
#define ST7789_WIDTH 135
#define ST7789_HEIGHT 240
#define X_SHIFT 53
#define Y_SHIFT 40
#endif

#if ST7789_ROTATION == 1
#define ST7789_WIDTH 240
#define ST7789_HEIGHT 135
#define X_SHIFT 40
#define Y_SHIFT 52
#endif

#if ST7789_ROTATION == 2
#define ST7789_WIDTH 135
#define ST7789_HEIGHT 240
#define X_SHIFT 52
#define Y_SHIFT 40
#endif

#if ST7789_ROTATION == 3
#define ST7789_WIDTH 240
#define ST7789_HEIGHT 135
#define X_SHIFT 40
#define Y_SHIFT 53
#endif

#endif

#ifdef USING_240X240

#define ST7789_WIDTH 240
#define ST7789_HEIGHT 240

#if ST7789_ROTATION == 0
#define X_SHIFT 0
#define Y_SHIFT 80
#elif ST7789_ROTATION == 1
#define X_SHIFT 80
#define Y_SHIFT 0
#elif ST7789_ROTATION == 2
#define X_SHIFT 0
#define Y_SHIFT 0
#elif ST7789_ROTATION == 3
#define X_SHIFT 0
#define Y_SHIFT 0
#endif

#endif

#ifdef USING_240X282

#if ST7789_ROTATION == 0
#define ST7789_WIDTH 240
#define ST7789_HEIGHT 282
#define X_SHIFT 0
#define Y_SHIFT 18
#elif ST7789_ROTATION == 1
#define ST7789_WIDTH 282
#define ST7789_HEIGHT 240
#define X_SHIFT 18
#define Y_SHIFT 0
#elif ST7789_ROTATION == 2
#define ST7789_WIDTH 240
#define ST7789_HEIGHT 282
#define X_SHIFT 0
#define Y_SHIFT 18
#elif ST7789_ROTATION == 3
#define ST7789_WIDTH 282
#define ST7789_HEIGHT 240
#define X_SHIFT 18
#define Y_SHIFT 0
#endif

#endif

#ifdef USING_170X320

#if ST7789_ROTATION == 0
#define ST7789_WIDTH 170
#define ST7789_HEIGHT 320
#define X_SHIFT 35
#define Y_SHIFT 0
#endif

#if ST7789_ROTATION == 1
#define ST7789_WIDTH 320
#define ST7789_HEIGHT 170
#define X_SHIFT 0
#define Y_SHIFT 35
#endif

#if ST7789_ROTATION == 2
#define ST7789_WIDTH 170
#define ST7789_HEIGHT 320
#define X_SHIFT 35
#define Y_SHIFT 0
#endif

#if ST7789_ROTATION == 3
#define ST7789_WIDTH 320
#define ST7789_HEIGHT 170
#define X_SHIFT 0
#define Y_SHIFT 35
#endif

#endif

// Color
#define WHITE 0xFFFF
#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define MAGENTA 0xF81F
#define GREEN 0x07E2
#define CYAN 0x7FFF
#define YELLOW 0xFF20
#define ORANGE 0xFB60
#define GRAY 0X8430
#define BRED 0XF81F
#define GRED 0XFFE0
#define GBLUE 0X07FF
#define BROWN 0XBC40
#define BRRED 0XFC07
#define PURPLE 0XF81F

#define DARKBLUE 0X01CF
#define LIGHTBLUE 0X7D7C
#define GRAYBLUE 0X5458

#define DARKGREEN 0x0320
#define LIGHTGREEN 0x9772
#define LGRAY 0XC618
#define LGRAYBLUE 0XA651
#define LBBLUE 0X2B12

#define LIGHTORANGE 0xFEB6
#define LIGHTPURPLE 0xDD7F
#define LIGHTBROWN 0xEE55

#define GREENORS 0x07EE
#define PINKORS 0xF811
#define YELLOWORS 0xFF20

/* Control Registers and constant codes */
#define ST7789_NOP 0x00
#define ST7789_SWRESET 0x01
#define ST7789_RDDID 0x04
#define ST7789_RDDST 0x09

#define ST7789_SLPIN 0x10
#define ST7789_SLPOUT 0x11
#define ST7789_PTLON 0x12
#define ST7789_NORON 0x13

#define ST7789_INVOFF 0x20
#define ST7789_INVON 0x21
#define ST7789_DISPOFF 0x28
#define ST7789_DISPON 0x29
#define ST7789_CASET 0x2A
#define ST7789_RASET 0x2B
#define ST7789_RAMWR 0x2C
#define ST7789_RAMRD 0x2E

#define ST7789_PTLAR 0x30
#define ST7789_COLMOD 0x3A
#define ST7789_MADCTL 0x36

/**
 * Memory Data Access Control Register (0x36H)
 * MAP:     D7  D6  D5  D4  D3  D2  D1  D0
 * param:   MY  MX  MV  ML  RGB MH  -   -
 *
 */

/* Page Address Order ('0': Top to Bottom, '1': the opposite) */
#define ST7789_MADCTL_MY 0x80
/* Column Address Order ('0': Left to Right, '1': the opposite) */
#define ST7789_MADCTL_MX 0x40
/* Page/Column Order ('0' = Normal Mode, '1' = Reverse Mode) */
#define ST7789_MADCTL_MV 0x20
/* Line Address Order ('0' = LCD Refresh Top to Bottom, '1' = the opposite) */
#define ST7789_MADCTL_ML 0x10
/* RGB/BGR Order ('0' = RGB, '1' = BGR) */
#define ST7789_MADCTL_RGB 0x00

#define ST7789_RDID1 0xDA
#define ST7789_RDID2 0xDB
#define ST7789_RDID3 0xDC
#define ST7789_RDID4 0xDD

/* Advanced options */
#define ST7789_COLOR_MODE_16bit 0x55 //  RGB565 (16bit)
#define ST7789_COLOR_MODE_18bit 0x66 //  RGB666 (18bit)

/* Basic operations */
#define ST7789_RST_Clr() HAL_GPIO_WritePin(ST7789_RST_PORT, ST7789_RST_PIN, GPIO_PIN_RESET)
#define ST7789_RST_Set() HAL_GPIO_WritePin(ST7789_RST_PORT, ST7789_RST_PIN, GPIO_PIN_SET)

#define ST7789_DC_Clr() HAL_GPIO_WritePin(ST7789_DC_PORT, ST7789_DC_PIN, GPIO_PIN_RESET)
#define ST7789_DC_Set() HAL_GPIO_WritePin(ST7789_DC_PORT, ST7789_DC_PIN, GPIO_PIN_SET)
#ifndef CFG_NO_CS
#define ST7789_Select() HAL_GPIO_WritePin(ST7789_CS_PORT, ST7789_CS_PIN, GPIO_PIN_RESET)
#define ST7789_UnSelect() HAL_GPIO_WritePin(ST7789_CS_PORT, ST7789_CS_PIN, GPIO_PIN_SET)
#else
#define ST7789_Select() asm("nop")
#define ST7789_UnSelect() asm("nop")
#endif
#define ST7789_BLK_Clr() HAL_GPIO_WritePin(ST7789_BLK_PORT, ST7789_BLK_PIN, GPIO_PIN_RESET)
#define ST7789_BLK_Set() HAL_GPIO_WritePin(ST7789_BLK_PORT, ST7789_BLK_PIN, GPIO_PIN_SET)

#define ABS(x) ((x) > 0 ? (x) : -(x))

/* Basic functions. */
void ST7789_Init(void); // 初始化
void ST7789_SetRotation(uint8_t m);
void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color);

#if defined(BL_PWM) || defined(BL_BIN)
void ST7789_SetBacklight(uint8_t);
#endif

/* */
void ST7789_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void ST7789_WriteData(uint8_t *buff, size_t buff_size);

/* Command functions */
void ST7789_TearEffect(uint8_t tear); //

/* Utils functions */
void wmemset(uint16_t *destination, uint16_t data, size_t size);
void ST7789_DrawChinese24x24(uint16_t x, uint16_t y, const uint8_t chinese[][72], uint16_t textLen, uint16_t textColor, uint16_t bgColor);
/* Simple test function. */
void ICA_remote_control(void);
void ST7789_sleep(void);

#ifndef ST7789_ROTATION
#error You should at least choose a display rotation!
#endif

#endif
