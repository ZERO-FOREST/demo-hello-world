# I2S TDM 立体声双MAX98357 + 单麦克风驱动

## 概述

这个I2S TDM驱动支持立体声双MAX98357音频放大器输出和单麦克风输入，使用TDM（Time Division Multiplexing）模式实现多通道音频传输。

## 硬件连接

### 引脚定义
```c
#define I2S_TDM_BCLK_PIN       7    // 位时钟
#define I2S_TDM_LRCK_PIN       8    // 帧时钟/WS
#define I2S_TDM_DATA_OUT_PIN   15   // 数据输出 (到双MAX98357)
#define I2S_TDM_DATA_IN_PIN    17   // 数据输入 (来自麦克风)
```

### 硬件连接图
```
ESP32-S3                    MAX98357 #1 (左声道)
BCLK (GPIO7) ────────────── BCLK
LRCK (GPIO8) ────────────── LRCK
DATA_OUT (GPIO15) ───────── DIN

ESP32-S3                    MAX98357 #2 (右声道)
BCLK (GPIO7) ────────────── BCLK
LRCK (GPIO8) ────────────── LRCK
DATA_OUT (GPIO15) ───────── DIN

ESP32-S3                    麦克风模块
BCLK (GPIO7) ────────────── BCLK
LRCK (GPIO8) ────────────── LRCK
DATA_IN (GPIO17) ────────── DOUT
```

## TDM配置

### 时隙分配
- **时隙0**: 左声道 (MAX98357 #1)
- **时隙1**: 右声道 (MAX98357 #2)  
- **时隙2**: 麦克风

### 音频参数
```c
#define I2S_TDM_SAMPLE_RATE    44100     // 采样率 44.1kHz
#define I2S_TDM_BITS_PER_SAMPLE 16       // 16位采样
#define I2S_TDM_CHANNELS       3         // TDM通道数
#define I2S_TDM_SLOT_BIT_WIDTH 32       // 物理时隙宽度
#define I2S_TDM_SLOT_NUM       3         // TDM时隙数量
```

## API使用

### 初始化
```c
#include "i2s_tdm.h"

// 初始化I2S TDM
esp_err_t ret = i2s_tdm_init();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init I2S TDM");
    return ret;
}

// 启动I2S TDM
ret = i2s_tdm_start();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start I2S TDM");
    return ret;
}
```

### 音频输出（立体声）
```c
// 立体声音频数据格式: [左声道16bit][右声道16bit][左声道16bit][右声道16bit]...
int16_t stereo_data[1024 * 2]; // 1024个立体声样本

// 生成音频数据
for (int i = 0; i < 1024; i++) {
    stereo_data[i * 2] = left_channel[i];   // 左声道
    stereo_data[i * 2 + 1] = right_channel[i]; // 右声道
}

// 写入音频数据
size_t bytes_written = 0;
esp_err_t ret = i2s_tdm_write(stereo_data, sizeof(stereo_data), &bytes_written);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to write audio data");
}
```

### 麦克风输入
```c
// 麦克风数据格式: [麦克风16bit][麦克风16bit][麦克风16bit]...
int16_t mic_data[1024]; // 1024个麦克风样本

// 读取麦克风数据
size_t bytes_read = 0;
esp_err_t ret = i2s_tdm_read(mic_data, sizeof(mic_data), &bytes_read);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read microphone data");
}

// 处理麦克风数据
uint32_t samples = bytes_read / sizeof(int16_t);
for (uint32_t i = 0; i < samples; i++) {
    int16_t mic_sample = mic_data[i];
    // 处理音频数据...
}
```

### 设置采样率
```c
// 动态设置采样率
esp_err_t ret = i2s_tdm_set_sample_rate(48000);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set sample rate");
}
```

### 停止和清理
```c
// 停止I2S TDM
i2s_tdm_stop();

// 反初始化
i2s_tdm_deinit();
```

## 演示程序

项目包含两个演示程序：

1. **`i2s_tdm_demo.c`** - 完整演示程序
   - 立体声播放: 播放"Twinkle Twinkle Little Star"旋律到双MAX98357
   - 麦克风监听: 实时读取麦克风数据并显示音频电平

2. **`i2s_tdm_simple_test.c`** - 简化测试程序
   - 播放440Hz测试音调
   - 麦克风电平监控
   - 用于调试"得得得"声音问题

### 运行演示
```c
#include "i2s_tdm_demo.h"
// 或者
#include "i2s_tdm_simple_test.h"

// 启动演示
esp_err_t ret = i2s_tdm_demo_init();
// 或者
esp_err_t ret = i2s_tdm_simple_test_init();

if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start demo");
    return ret;
}

// 停止演示
i2s_tdm_demo_deinit();
// 或者
i2s_tdm_simple_test_deinit();
```

## 技术特性

### TDM模式优势
- **多通道支持**: 单根数据线传输3个音频通道
- **同步传输**: 所有通道使用相同的时钟，确保同步
- **高效利用**: 减少引脚使用，简化PCB设计

### 音频质量
- **16位分辨率**: 提供良好的动态范围
- **44.1kHz采样率**: 支持CD音质
- **立体声输出**: 左右声道独立控制
- **单麦克风输入**: 支持音频采集

### 性能优化
- **DMA传输**: 减少CPU占用
- **缓冲区管理**: 自动处理音频缓冲
- **实时处理**: 低延迟音频传输

## 故障排除

### 常见问题

#### 1. "得得得"声音问题
**症状**: 听到持续的"得得得"或"咔咔咔"声音

**可能原因**:
- 时钟配置不匹配
- 数据格式错误
- 硬件连接问题
- 电源不稳定

**解决方案**:
1. **检查时钟源**: 使用XTAL时钟而不是APLL
   ```c
   clk_cfg.clk_src = I2S_CLK_SRC_XTAL;  // 更稳定
   ```

2. **验证硬件连接**:
   - 确保BCLK、LRCK、DATA_OUT、DATA_IN正确连接
   - 检查电源电压是否稳定
   - 确认MAX98357的增益设置

3. **调整音频参数**:
   - 降低音频音量（amplitude = 0.1f）
   - 检查采样率是否匹配
   - 验证数据位宽设置

4. **使用简化测试程序**:
   ```c
   #include "i2s_tdm_simple_test.h"
   i2s_tdm_simple_test_init();  // 使用简化的测试
   ```

#### 2. 无声音输出
**检查项目**:
- 硬件连接是否正确
- MAX98357电源是否正常
- 音频数据格式是否正确
- 音量设置是否合适

#### 3. 麦克风无信号
**检查项目**:
- 麦克风电源连接
- 数据线连接
- 麦克风增益设置
- 采样率配置

#### 4. 编译错误
**解决方案**:
- 确保ESP-IDF版本兼容
- 检查头文件包含
- 验证函数声明

### 调试步骤

1. **使用简化测试程序**:
   ```c
   i2s_tdm_simple_test_init();
   ```

2. **检查串口输出**:
   - 查看初始化日志
   - 监控音频电平
   - 检查错误信息

3. **逐步测试**:
   - 先测试音频输出
   - 再测试麦克风输入
   - 最后测试完整功能

4. **硬件验证**:
   - 用示波器检查BCLK、LRCK信号
   - 验证DATA_OUT、DATA_IN信号
   - 检查电源纹波

### 调试信息
驱动会输出详细的调试信息，包括：
- 初始化状态
- 硬件配置
- 音频参数
- 错误信息
- 音频电平监控

## 注意事项

1. **硬件连接**: 确保BCLK、LRCK、DATA_OUT、DATA_IN正确连接
2. **电源管理**: MAX98357需要稳定的电源供应
3. **时钟同步**: 所有设备必须使用相同的时钟源
4. **数据格式**: 注意音频数据的字节序和对齐方式
5. **缓冲区大小**: 根据应用需求调整缓冲区大小
6. **音量控制**: 避免音频过载，建议使用较低的音量

## 版本历史

- **v1.1**: 优化版本，支持单麦克风，修复"得得得"声音问题
  - 使用XTAL时钟源，提高稳定性
  - 简化TDM配置，减少时隙数量
  - 添加简化测试程序
  - 优化音频参数配置

- **v1.0**: 初始版本，支持立体声双MAX98357 + 双麦克风
  - 支持TDM模式
  - 支持16位/44.1kHz音频
  - 包含完整演示程序
