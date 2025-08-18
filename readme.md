# ESP32-S3 Demo Hello World

## 项目简介

本项目基于 ESP32-S3 R8N16 开发板，演示了多种功能，包括图像传输、串口打印、陀螺仪体感、小型游戏及远程通话（蓝牙 Mesh 实现）。适用于物联网、智能硬件等场景的快速开发和学习。

## 硬件

- ESP32-S3 R8N16 开发板

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