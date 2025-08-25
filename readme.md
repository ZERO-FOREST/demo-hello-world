# ESP32-S3 Demo Hello World

## 项目简介

本项目基于 ESP32-S3 R8N16 开发板，演示了多种功能，包括图像传输、串口打印、陀螺仪体感、小型游戏及远程通话（蓝牙 Mesh 实现）。适用于物联网、智能硬件等场景的快速开发和学习。

## 硬件

- ESP32-S3 R8N16 开发板

## 接收端

### A 面（信号为主，给高速留“地护栏”）

A1：GND

A2：GND

A3：+5V（USB/VBUS 或备用 5V）

A4：GND（可改作 USB_ID/OTG，默认 GND 做屏蔽）

A5：GND（USB 护栏）

A6：USB_D+

A7：USB_D−（A5/A8 做护栏）

A8：GND（USB 护栏）

A9：GND（SCLK 护栏）

A10：SPI_SCLK（两侧 A9/A11 为 GND）

A11：GND（SCLK 护栏）

A12：SPI_MOSI

A13：GND（MOSIx 护栏）

A14：SPI_MISO

A15：GND（MISO 护栏）

A16：SPI_CS0

A17：+3V3（主逻辑电源）

A18：GND

### B 面（PWM+CS 分区，穿插接地）

B1：GND

B2：GND

B3：+5V（与 A3 并联提高承载）

B4：SPI_CS1（或备用 CS/INT）

B5：PWM1

B6：GND

B7：PWM2

B8：PWM3

B9：GND

B10：PWM4

B11：PWM5

B12：GND

B13：PWM6

B14：PWM7

B15：GND

B16：PWM8

B17：+3V3（与 A17 并联）

B18：GND

## 支持功能

1. 图像传输
2. 串口打印
3. 陀螺仪体感
4. 小游戏
5. 远程通话（蓝牙 Mesh 实现）

## 目录结构

- `main/`：主程序代码，包括入口文件和各功能模块
- `components/`：自定义组件及第三方库
- `build/`：编译生成的中间文件和固件
- `doc/`：相关文档说明
- `managed_components/`：通过组件管理器安装的依赖
- `others/`：其他辅助文件或测试代码

## 开发环境

ESP32-IDF v5.5 和 ESP-ADF库

## 安装中文字库

1. 确保工程目录下存在font文件夹，里面包含现有的字库文件
2. 查看分区是否正确，字库分区为1536K大小，在[分区表](./partitions.csv)中查看
3. 打开VS-Code的ESP-IDF工具，在设备分区资源管理器中选中font，下载二进制文件到该分区中，二进制文件为.bin字体文件，如果不喜欢该字体，可以更换为自己喜欢的字体。使用LVGL的[字体转换网站](https://lvgl.io/tools/fontconverter)制作即可。

### 字体制作指南

|选项|提示|
| :--- | :----------------------- |
|Size|一般标题为20，常规显示为16|
|Bpp|2|
|Range|0x20-0x7F,0x4E00-0x9FA5|

其他选择自己喜欢的中文字体即可，上述的范围包括常规的汉字和数字。通过网页转换如果字体文件较大的话会卡顿，耐心等待。

## 编译与烧录

1. 安装 ESP-IDF 开发环境（建议使用官方文档步骤）
2. 在项目根目录下执行：
   ```powershell
   idf.py build
   idf.py -p <COM端口号> flash
   ```
3. 连接开发板，打开串口工具查看输出

### 更新计划

1. 后续支持ELRS协议，支持串行多通道PWM传输
2. 支持接收端4通道PWM直出
3. 接收端将采用PCIE接口设计，目前想法为pcie x1 接口

## 参考文档

- [ESP-IDF 官方文档](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/index.html)
- 项目内 `doc/` 文件夹