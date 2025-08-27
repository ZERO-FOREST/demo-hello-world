该文件夹用于存放接收端的相关工程文件

## 功能

### 图片编码

使用ESP-ADF的 `new_jpeg` 库进行实时解编码。

### 协议解析

### PWM输出

### 原生协议输出

## 通信方式

支持USB和SPI通信

## 编译方式

在顶层的[Cmakelist.txt](../../CMakeLists.txt)文件中修改 `EN_RECEIVER_MODE` 为ON即可

## 引脚分配

### A 面（信号为主，给高速留"地护栏"）

| 引脚 | 功能 | 说明 |
|------|------|------|
| A1 | GND | |
| A2 | GND | |
| A3 | +5V | USB/VBUS 或备用 5V |
| A4 | GND | 可改作 USB_ID/OTG，默认 GND 做屏蔽 |
| A5 | GND | USB 护栏 |
| A6 | USB_D+ | |
| A7 | USB_D− | A5/A8 做护栏 |
| A8 | GND | USB 护栏 |
| A9 | GND | SCLK 护栏 |
| A10 | SPI_SCLK | 两侧 A9/A11 为 GND |
| A11 | GND | SCLK 护栏 |
| A12 | SPI_MOSI | |
| A13 | GND | MOSIx 护栏 |
| A14 | SPI_MISO | |
| A15 | GND | MISO 护栏 |
| A16 | SPI_CS0 | |
| A17 | +3V3 | 主逻辑电源 |
| A18 | GND | |

### B 面（PWM+CS 分区，穿插接地）

| 引脚 | 功能 | 说明 |
|------|------|------|
| B1 | GND | |
| B2 | GND | |
| B3 | +5V | 与 A3 并联提高承载 |
| B4 | SPI_CS1 | 或备用 CS/INT |
| B5 | PWM1 | |
| B6 | GND | |
| B7 | PWM2 | |
| B8 | PWM3 | |
| B9 | GND | |
| B10 | PWM4 | |
| B11 | PWM5 | |
| B12 | GND | |
| B13 | PWM6 | |
| B14 | PWM7 | |
| B15 | GND | |
| B16 | PWM8 | |
| B17 | +3V3 | 与 A17 并联 |
| B18 | GND | |

