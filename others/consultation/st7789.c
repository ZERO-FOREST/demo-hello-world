#include "st7789.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "menu.h"

// Modifies the SPI clk when another slow device is present on the same line, such as an SD card.
#define FCLK_FASTER()                                                                        \
	{                                                                                        \
		MODIFY_REG(hspi1.Instance->CR1, SPI_BAUDRATEPRESCALER_256, SPI_BAUDRATEPRESCALER_2); \
	} /* Set SCLK = fast, approx 32 MBits/s */
/**
 * @brief Write command to ST7789 controller
 * @param cmd -> command to write
 * @return none
 */
static void ST7789_WriteCommand(uint8_t cmd)
{
	FCLK_FASTER();
	ST7789_Select();
	ST7789_DC_Clr();
	HAL_SPI_Transmit_DMA(&ST7789_SPI_PORT, &cmd, sizeof(cmd));

	ST7789_UnSelect();
}

/**
 * @brief Write data to ST7789 controller
 * @param buff -> pointer of data buffer
 * @param buff_size -> size of the data buffer
 * @return none
 */
void ST7789_WriteData(uint8_t *buff, size_t buff_size)
{
	FCLK_FASTER();
	ST7789_Select();
	ST7789_DC_Set();

	while (buff_size > 0)
	{
		uint16_t chunk_size = buff_size > 0xFFFF ? 0xFFFF : buff_size;
#ifdef USE_DMA
		HAL_SPI_Transmit_DMA(&ST7789_SPI_PORT, buff, chunk_size);

		while (ST7789_SPI_PORT.State != HAL_SPI_STATE_READY)
			;

#else
		HAL_SPI_Transmit(&ST7789_SPI_PORT, buff, chunk_size, HAL_MAX_DELAY);

#endif
		buff += chunk_size;
		buff_size -= chunk_size;
	}

	ST7789_UnSelect();
}
/**
 * @brief Write data to ST7789 controller, simplify for 8bit data.
 * data -> data to write
 * @return none
 */
static void ST7789_WriteSmallData(uint8_t data)
{
	FCLK_FASTER();
	ST7789_Select();
	ST7789_DC_Set();
	/*TODO*/ HAL_SPI_Transmit(&ST7789_SPI_PORT, &data, sizeof(data), /*TODO*/
							  HAL_MAX_DELAY);
	//	LL_SPI_TransmitData8(&ST7789_SPI_PORT, data);
	ST7789_UnSelect();
}

/**
 * @brief Set the rotation direction of the display
 * @param m -> rotation parameter(please refer it in st7789.h)
 * @return none
 */
void ST7789_SetRotation(uint8_t m)
{
	ST7789_WriteCommand(ST7789_MADCTL); // MADCTL
	switch (m)
	{
	case 0:
		ST7789_WriteSmallData(
			ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_RGB);
		break;
	case 1:
		ST7789_WriteSmallData(
			ST7789_MADCTL_MY | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
		break;
	case 2:
		ST7789_WriteSmallData(ST7789_MADCTL_RGB);
		break;
	case 3:
		ST7789_WriteSmallData(
			ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
		break;
	default:
		break;
	}
}

/**
 * @brief Set address of DisplayWindow
 * @param xi&yi -> coordinates of window
 * @return none
 */
void ST7789_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
	uint16_t x_start = x0 + X_SHIFT, x_end = x1 + X_SHIFT;
	uint16_t y_start = y0 + Y_SHIFT, y_end = y1 + Y_SHIFT;

	/* Column Address set */
	ST7789_WriteCommand(ST7789_CASET);
	{
		uint8_t data[] =
			{x_start >> 8, x_start & 0xFF, x_end >> 8, x_end & 0xFF};
		ST7789_WriteData(data, sizeof(data));
	}

	/* Row Address set */
	ST7789_WriteCommand(ST7789_RASET);
	{
		uint8_t data[] =
			{y_start >> 8, y_start & 0xFF, y_end >> 8, y_end & 0xFF};
		ST7789_WriteData(data, sizeof(data));
	}
	/* Write to RAM */
	ST7789_WriteCommand(ST7789_RAMWR);
}

/**
 * @brief Fill the DisplayWindow with single color
 * @param color -> color to Fill with
 * @return none
 */
void ST7789_Fill_Color(uint16_t color)
{
	uint16_t i;
	ST7789_SetAddressWindow(0, 0, ST7789_WIDTH - 1, ST7789_HEIGHT - 1);
	ST7789_Select();
	uint16_t j;
	uint8_t data[] = {color >> 8, color & 0xFF};
	for (i = 0; i < ST7789_WIDTH; i++)
		for (j = 0; j < ST7789_HEIGHT; j++)
		{
			ST7789_WriteData(data, sizeof(data));
		}
	ST7789_UnSelect();
}

/**
 * @brief Initialize ST7789 controller
 * @param none
 * @return none
 */
void ST7789_Init(void)
{
	ST7789_RST_Clr();
	HAL_Delay(100);
	ST7789_RST_Set();
	HAL_Delay(100);

	ST7789_BLK_Set();
	HAL_Delay(100);

	ST7789_WriteCommand(0x11); //	Sleep out
	HAL_Delay(120);

	ST7789_WriteCommand(0x36);
	ST7789_SetRotation(ST7789_ROTATION); //	MADCTL (Display Rotation)显示旋转

	ST7789_WriteCommand(ST7789_COLMOD); //	Set color mode
	ST7789_WriteSmallData(0x05);

	ST7789_WriteCommand(0xB2); //	Porch control
	{
		uint8_t data[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
		ST7789_WriteData(data, sizeof(data));
	}

	/* Internal LCD Voltage generator settings */
	/* 内部LCD电压发生器�?�置 */
	ST7789_WriteCommand(0XB7);	 //	Gate Control
	ST7789_WriteSmallData(0x35); //	Default value
	ST7789_WriteCommand(0xBB);	 //	VCOM setting
	ST7789_WriteSmallData(0x32); //	0.725v (default 0.75v for 0x20)
	ST7789_WriteCommand(0xC2);	 //	LCMCTRL
	ST7789_WriteSmallData(0x01); //	Default value
	ST7789_WriteCommand(0xC3);	 //	VDV and VRH command Enable
	ST7789_WriteSmallData(0x15); //	Default value
	ST7789_WriteCommand(0xC4);	 //	VRH set
	ST7789_WriteSmallData(0x20); //	+-4.45v (defalut +-4.1v for 0x0B)
	ST7789_WriteCommand(0xC6);	 //	VDV set
	ST7789_WriteSmallData(0x0F); //	Default value
	ST7789_WriteCommand(0xD0);	 //	Power control
	ST7789_WriteSmallData(0xA4); //	Default value
	ST7789_WriteSmallData(0xA1); //	Default value
	/**************** Division line ****************/

	ST7789_WriteCommand(0xE0);
	{
		uint8_t data[] = {0xD0, 0x08, 0x0E, 0x09, 0x09, 0x05, 0x31, 0x33, 0x48, 0x17, 0x14, 0X15, 0x31, 0x34};
		ST7789_WriteData(data, sizeof(data));
	}

	ST7789_WriteCommand(0xE1);
	{
		uint8_t data[] = {0xD0, 0x08, 0x0E, 0x09, 0x09, 0x05, 0x31, 0x33, 0x48, 0x17, 0x14, 0X15, 0x31, 0x34};
		ST7789_WriteData(data, sizeof(data));
	}

	ST7789_WriteCommand(ST7789_INVON);	//	Inversion ON
	ST7789_WriteCommand(ST7789_SLPOUT); //	Out of sleep mode
	ST7789_WriteCommand(ST7789_NORON);	//	Normal Display on
	ST7789_WriteCommand(ST7789_DISPON); //	Main screen turned on

	// ST7789_Fill_Color(BLACK);
}

/**
 * @brief Draw a Pixel
 * @param x&y -> coordinate to Draw
 * @param color -> color of the Pixel
 * @return none
 */
void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
	if ((x < 0) || (x >= ST7789_WIDTH) || (y < 0) || (y >= ST7789_HEIGHT))
		return;

	ST7789_SetAddressWindow(x, y, x, y);
	uint8_t data[] =
		{color >> 8, color & 0xFF};
	ST7789_Select();
	ST7789_WriteData(data, sizeof(data));
	ST7789_UnSelect();
}

#ifdef BL_PWM
/**
 * @brief
 *
 * @param duty 背光灯亮度 0-255
 */
void ST7789_SetBacklight(uint8_t duty)
{
#ifdef CHxN
	__HAL_TIM_SET_COMPARE(&ST7789_PWM_TIM, ST7789_PWM_CH, 255 - duty);
#else
	__HAL_TIM_SET_COMPARE(&ST7789_PWM_TIM, ST7789_PWM_CH, duty);
#endif
}
#elif BL_BIN
void ST7789_SetBacklight(uint8_t stat)
{
	HAL_GPIO_WritePin(TFT_BL_GPIO_Port, TFT_BL_Pin, stat);
}
#endif

void ST7789_sleep(void)
{
	ST7789_Select();
	ST7789_WriteCommand(0x10);
	HAL_Delay(120);
	ST7789_UnSelect();
}
