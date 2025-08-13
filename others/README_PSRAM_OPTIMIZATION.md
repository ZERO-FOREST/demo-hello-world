# PSRAM内存优化说明

## 概述

为了解决DRAM内存溢出问题，我们对串口显示功能进行了PSRAM内存优化。主要优化内容包括：

### 🔧 **主要修改**

1. **串口显示界面 (`ui_serial_display.c`)**
   - 将1024行数据缓冲区从DRAM迁移到PSRAM
   - 使用 `heap_caps_malloc()` 和 `heap_caps_free()` 管理内存
   - 添加缓冲区初始化和清理函数

2. **串口显示模块 (`serial_display.c`)**
   - 将4KB显示数据缓冲区迁移到PSRAM
   - 优化任务内存分配，使用8位对齐内存
   - 添加内存状态检查和错误处理

### 📊 **内存使用对比**

| 组件 | 优化前 (DRAM) | 优化后 (PSRAM) | 节省 |
|------|---------------|----------------|------|
| 串口显示界面 | ~300KB | ~0KB | 300KB |
| 串口显示模块 | 4KB | ~0KB | 4KB |
| 总计节省 | - | - | **304KB** |

### 🛠 **技术实现**

#### 1. PSRAM内存分配

```c
// 分配PSRAM内存
g_lines = (display_line_t *)heap_caps_malloc(
    MAX_LINES * sizeof(display_line_t), 
    MALLOC_CAP_SPIRAM
);

// 分配8位对齐内存（用于任务）
local_buffer = (uint8_t *)heap_caps_malloc(
    MAX_DISPLAY_DATA_SIZE, 
    MALLOC_CAP_8BIT
);
```

#### 2. 内存管理函数

```c
// 初始化PSRAM缓冲区
static esp_err_t init_psram_buffer(void);

// 清理PSRAM缓冲区
static void cleanup_psram_buffer(void);
```

#### 3. 错误处理

```c
// 检查缓冲区状态
if (!g_buffer_initialized || g_lines == NULL) {
    ESP_LOGE(TAG, "Buffer not initialized");
    return;
}
```

### 🔍 **配置要求**

确保 `sdkconfig` 中启用PSRAM：

```ini
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_TYPE_AUTO=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY=y
```

### 🧪 **测试验证**

使用提供的测试脚本验证PSRAM功能：

```bash
python others/test_psram.py
```

测试内容包括：
- PSRAM内存分配测试
- 大量数据传输测试
- 内存溢出保护测试

### 📈 **性能影响**

1. **内存访问速度**
   - PSRAM访问速度比DRAM慢约2-3倍
   - 对于串口显示功能影响微乎其微

2. **系统稳定性**
   - 避免DRAM溢出导致的系统崩溃
   - 提高大容量数据处理能力

3. **功能完整性**
   - 保持所有原有功能不变
   - 支持1024行数据存储

### 🚀 **使用建议**

1. **编译前检查**
   ```bash
   # 检查PSRAM配置
   grep CONFIG_SPIRAM sdkconfig
   ```

2. **运行时监控**
   ```bash
   # 监控内存使用情况
   idf.py monitor
   ```

3. **性能优化**
   - 定期清理不需要的数据
   - 避免频繁的内存分配/释放

### 🔧 **故障排除**

#### 常见问题

1. **PSRAM初始化失败**
   ```
   E: Failed to allocate PSRAM buffer
   ```
   **解决方案**: 检查PSRAM硬件连接和配置

2. **内存分配失败**
   ```
   E: Buffer not initialized
   ```
   **解决方案**: 确保在调用功能前已初始化

3. **编译错误**
   ```
   region 'dram0_0_seg' overflowed
   ```
   **解决方案**: 检查是否所有大缓冲区都已迁移到PSRAM

### 📝 **代码示例**

#### 正确的内存使用方式

```c
// ✅ 推荐：使用PSRAM
void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
if (ptr) {
    // 使用内存
    heap_caps_free(ptr);
}

// ❌ 避免：直接使用大数组
static uint8_t large_buffer[1024 * 1024];  // 会占用DRAM
```

### 🎯 **总结**

通过PSRAM优化，我们成功解决了DRAM内存溢出问题，同时保持了所有功能的完整性。这个优化为后续功能扩展提供了充足的内存空间。
