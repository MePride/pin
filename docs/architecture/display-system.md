# Pin 显示系统设计文档

## 概述

Pin 显示系统基于 FPC-A005 2.9英寸彩色电子墨水屏设计，支持七色显示，具备超低功耗特性和优秀的可视性。

## 硬件规格

### FPC-A005 电子墨水屏规格
- **尺寸**: 2.9英寸
- **分辨率**: 600 × 448 像素
- **颜色**: 7色显示 (黑、白、红、黄、蓝、绿、橙)
- **接口**: SPI
- **功耗**: 显示时约100mA，待机时约0.1µA
- **刷新时间**: 全屏刷新约15秒，部分刷新约2秒

### ESP32-C3 连接方式
```c
// SPI 引脚定义
#define EPD_SCK_PIN     GPIO_NUM_2
#define EPD_MOSI_PIN    GPIO_NUM_3  
#define EPD_CS_PIN      GPIO_NUM_10
#define EPD_DC_PIN      GPIO_NUM_4
#define EPD_RST_PIN     GPIO_NUM_5
#define EPD_BUSY_PIN    GPIO_NUM_6
```

## 显示系统架构

### 核心组件结构

```c
// 颜色定义
typedef struct {
    uint8_t black;      // 黑色 0x00
    uint8_t white;      // 白色 0x01
    uint8_t red;        // 红色 0x02
    uint8_t yellow;     // 黄色 0x03
    uint8_t blue;       // 蓝色 0x04
    uint8_t green;      // 绿色 0x05
    uint8_t orange;     // 橙色 0x06
} pin_colors_t;

// 刷新模式
typedef enum {
    PIN_REFRESH_FULL,     // 全屏刷新 (清屏+重绘)
    PIN_REFRESH_PARTIAL,  // 部分刷新 (快速更新)
    PIN_REFRESH_FAST      // 快速刷新 (局部区域)
} pin_refresh_mode_t;

// 显示区域定义
typedef struct {
    uint16_t x, y, width, height;    // 位置和尺寸
    pin_colors_t color;              // 颜色配置
    char* content;                   // 显示内容
    uint8_t font_size;               // 字体大小
    bool visible;                    // 是否可见
    bool dirty;                      // 是否需要更新
} pin_widget_region_t;

// 显示管理器
typedef struct {
    pin_widget_region_t regions[PIN_MAX_WIDGETS];  // 显示区域数组
    uint8_t region_count;                          // 区域数量
    uint32_t last_full_refresh;                    // 上次全屏刷新时间
    uint32_t refresh_interval;                     // 刷新间隔
    pin_refresh_mode_t default_mode;               // 默认刷新模式
    bool power_save_enabled;                       // 省电模式开关
    SemaphoreHandle_t display_mutex;               // 显示互斥锁
} pin_display_manager_t;
```

## 显示系统API

### 初始化和配置
```c
// 显示系统初始化
esp_err_t pin_display_init(void);

// 添加显示区域
esp_err_t pin_display_add_widget(pin_widget_region_t* widget);

// 移除显示区域
esp_err_t pin_display_remove_widget(uint8_t widget_id);

// 更新显示内容
esp_err_t pin_display_update_widget(uint8_t widget_id, const char* content);

// 设置显示属性
esp_err_t pin_display_set_widget_color(uint8_t widget_id, pin_colors_t color);
esp_err_t pin_display_set_widget_font(uint8_t widget_id, uint8_t font_size);
```

### 刷新控制
```c
// 刷新显示
esp_err_t pin_display_refresh(pin_refresh_mode_t mode);

// 设置刷新间隔
esp_err_t pin_display_set_refresh_interval(uint32_t seconds);

// 强制全屏刷新
esp_err_t pin_display_force_full_refresh(void);

// 清屏
esp_err_t pin_display_clear(pin_colors_t bg_color);
```

### 电源管理
```c
// 进入睡眠模式
esp_err_t pin_display_sleep(void);

// 唤醒显示
esp_err_t pin_display_wake(void);

// 设置省电模式
esp_err_t pin_display_set_power_save(bool enable);
```

## 刷新策略设计

### 三级刷新机制

```c
// 刷新策略配置
typedef struct {
    uint32_t fast_refresh_interval;        // 快速刷新间隔 (30秒)
    uint32_t partial_refresh_interval;     // 部分刷新间隔 (5分钟)  
    uint32_t full_refresh_interval;        // 全屏刷新间隔 (30分钟)
    uint32_t sleep_after_inactive;         // 无活动后进入睡眠 (10分钟)
    uint8_t max_partial_refresh;           // 最大部分刷新次数后强制全屏刷新
    bool auto_refresh_enabled;             // 自动刷新开关
} pin_display_config_t;

// 默认配置
static pin_display_config_t default_config = {
    .fast_refresh_interval = 30,      // 30秒
    .partial_refresh_interval = 300,  // 5分钟
    .full_refresh_interval = 1800,    // 30分钟
    .sleep_after_inactive = 600,      // 10分钟
    .max_partial_refresh = 10,        // 10次部分刷新后强制全屏
    .auto_refresh_enabled = true
};
```

### 刷新决策算法

```c
// 刷新决策函数
pin_refresh_mode_t pin_display_decide_refresh_mode(void) {
    uint32_t now = xTaskGetTickCount() / portTICK_PERIOD_MS / 1000;
    static uint8_t partial_refresh_count = 0;
    
    // 检查是否有脏区域需要更新
    bool has_dirty_regions = false;
    for (int i = 0; i < display_manager.region_count; i++) {
        if (display_manager.regions[i].dirty) {
            has_dirty_regions = true;
            break;
        }
    }
    
    if (!has_dirty_regions) {
        return PIN_REFRESH_NONE;
    }
    
    // 强制全屏刷新条件
    if (partial_refresh_count >= default_config.max_partial_refresh ||
        (now - display_manager.last_full_refresh) >= default_config.full_refresh_interval) {
        partial_refresh_count = 0;
        display_manager.last_full_refresh = now;
        return PIN_REFRESH_FULL;
    }
    
    // 部分刷新条件
    if ((now - display_manager.last_partial_refresh) >= default_config.partial_refresh_interval) {
        partial_refresh_count++;
        display_manager.last_partial_refresh = now;
        return PIN_REFRESH_PARTIAL;
    }
    
    // 快速刷新
    return PIN_REFRESH_FAST;
}
```

## 电源管理机制

### 深度睡眠实现

```c
// 深度睡眠配置
void pin_enter_deep_sleep(uint32_t sleep_seconds) {
    // 保存当前显示状态到RTC内存
    pin_display_save_state_to_rtc();
    
    // 关闭SPI外设
    spi_bus_free(HSPI_HOST);
    
    // 配置唤醒源
    esp_sleep_enable_timer_wakeup(sleep_seconds * 1000000ULL);
    esp_sleep_enable_ext0_wakeup(GPIO_BUTTON_0, 0);  // 按钮唤醒
    
    // 进入深度睡眠
    ESP_LOGI(TAG, "Entering deep sleep for %d seconds", sleep_seconds);
    esp_deep_sleep_start();
}

// 从深度睡眠恢复
void pin_wake_from_deep_sleep(void) {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    
    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_TIMER:
            ESP_LOGI(TAG, "Wakeup from timer");
            break;
        case ESP_SLEEP_WAKEUP_EXT0:
            ESP_LOGI(TAG, "Wakeup from button press");
            break;
        default:
            ESP_LOGI(TAG, "Wakeup from unknown source");
            break;
    }
    
    // 恢复显示状态
    pin_display_restore_state_from_rtc();
    pin_display_init();
}
```

### 电池监控

```c
// 电池电压监控
typedef struct {
    float voltage;              // 当前电压
    uint8_t percentage;         // 电量百分比
    bool low_battery_warning;   // 低电量警告
    uint32_t last_check_time;   // 上次检查时间
} pin_battery_info_t;

// 电池电压读取
float pin_battery_get_voltage(void) {
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    adc_oneshot_new_unit(&init_config, &adc_handle);
    
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_0, &config);
    
    int raw_value;
    adc_oneshot_read(adc_handle, ADC_CHANNEL_0, &raw_value);
    
    // 转换为实际电压 (考虑分压电路)
    float voltage = (raw_value * 3.3f / 4096.0f) * 2.0f;  // 假设1:1分压
    
    adc_oneshot_del_unit(adc_handle);
    return voltage;
}

// 电量百分比计算
uint8_t pin_battery_get_percentage(float voltage) {
    // 锂电池电压范围: 3.0V-4.2V
    if (voltage >= 4.2f) return 100;
    if (voltage <= 3.0f) return 0;
    
    return (uint8_t)((voltage - 3.0f) / 1.2f * 100.0f);
}
```

## 字体和图形系统

### 字体管理

```c
// 字体定义
typedef enum {
    PIN_FONT_SMALL = 12,    // 小字体
    PIN_FONT_MEDIUM = 16,   // 中字体  
    PIN_FONT_LARGE = 24,    // 大字体
    PIN_FONT_XLARGE = 32    // 超大字体
} pin_font_size_t;

// 内置字体支持
extern const uint8_t font_12x12[];   // 12x12 中文字体
extern const uint8_t font_16x16[];   // 16x16 中文字体
extern const uint8_t font_24x24[];   // 24x24 中文字体
extern const uint8_t ascii_8x16[];   // 8x16 ASCII字体

// 文本渲染函数
esp_err_t pin_display_draw_text(uint16_t x, uint16_t y, const char* text, 
                                pin_font_size_t font_size, pin_colors_t color);
```

### 图形绘制

```c
// 基础图形绘制
esp_err_t pin_display_draw_pixel(uint16_t x, uint16_t y, uint8_t color);
esp_err_t pin_display_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t color);
esp_err_t pin_display_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t color, bool filled);
esp_err_t pin_display_draw_circle(uint16_t x, uint16_t y, uint16_t r, uint8_t color, bool filled);

// 位图显示
esp_err_t pin_display_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* bitmap);
```

## 性能优化

### 显示缓冲区管理

```c
// 双缓冲机制
typedef struct {
    uint8_t* front_buffer;      // 前缓冲区(当前显示)
    uint8_t* back_buffer;       // 后缓冲区(准备显示)  
    bool buffer_ready;          // 后缓冲区就绪标志
    SemaphoreHandle_t buffer_mutex;  // 缓冲区互斥锁
} pin_display_buffer_t;

// 缓冲区交换
esp_err_t pin_display_swap_buffers(void) {
    if (xSemaphoreTake(display_buffer.buffer_mutex, pdMS_TO_TICKS(100))) {
        uint8_t* temp = display_buffer.front_buffer;
        display_buffer.front_buffer = display_buffer.back_buffer;
        display_buffer.back_buffer = temp;
        display_buffer.buffer_ready = false;
        xSemaphoreGive(display_buffer.buffer_mutex);
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}
```

### 局部更新优化

```c
// 脏区域跟踪
typedef struct {
    uint16_t min_x, min_y;      // 脏区域左上角
    uint16_t max_x, max_y;      // 脏区域右下角
    bool has_dirty_region;      // 是否有脏区域
} pin_dirty_region_t;

// 标记脏区域
esp_err_t pin_display_mark_dirty(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (!dirty_region.has_dirty_region) {
        dirty_region.min_x = x;
        dirty_region.min_y = y;
        dirty_region.max_x = x + w - 1;
        dirty_region.max_y = y + h - 1;
        dirty_region.has_dirty_region = true;
    } else {
        // 扩展脏区域
        dirty_region.min_x = MIN(dirty_region.min_x, x);
        dirty_region.min_y = MIN(dirty_region.min_y, y);
        dirty_region.max_x = MAX(dirty_region.max_x, x + w - 1);
        dirty_region.max_y = MAX(dirty_region.max_y, y + h - 1);
    }
    return ESP_OK;
}
```

## 调试和诊断

### 显示系统状态监控

```c
// 系统状态结构
typedef struct {
    uint32_t total_refreshes;       // 总刷新次数
    uint32_t full_refreshes;        // 全屏刷新次数
    uint32_t partial_refreshes;     // 部分刷新次数
    uint32_t last_refresh_time;     // 上次刷新时间
    float average_refresh_time;     // 平均刷新时间
    uint32_t memory_used;           // 显示系统内存使用
    pin_battery_info_t battery;     // 电池信息
} pin_display_stats_t;

// 获取显示统计信息
esp_err_t pin_display_get_stats(pin_display_stats_t* stats);

// 显示系统诊断
esp_err_t pin_display_self_test(void) {
    ESP_LOGI(TAG, "Starting display self-test...");
    
    // 测试颜色显示
    for (int color = 0; color < 7; color++) {
        pin_display_clear(color);
        pin_display_refresh(PIN_REFRESH_FULL);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    
    // 测试文本显示
    pin_display_clear(PIN_COLOR_WHITE);
    pin_display_draw_text(50, 100, "Pin Display Test", PIN_FONT_LARGE, PIN_COLOR_BLACK);
    pin_display_refresh(PIN_REFRESH_FULL);
    
    ESP_LOGI(TAG, "Display self-test completed");
    return ESP_OK;
}
```

## 使用示例

### 基础使用流程

```c
void app_main(void) {
    // 初始化显示系统
    pin_display_init();
    
    // 创建时钟显示区域
    pin_widget_region_t clock_widget = {
        .x = 50, .y = 50, .width = 200, .height = 100,
        .color = {.black = 255},
        .content = "12:34",
        .font_size = PIN_FONT_XLARGE,
        .visible = true,
        .dirty = true
    };
    
    pin_display_add_widget(&clock_widget);
    
    // 主循环
    while (1) {
        // 更新时间
        time_t now = time(NULL);
        struct tm* timeinfo = localtime(&now);
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%H:%M", timeinfo);
        
        pin_display_update_widget(0, time_str);
        
        // 根据策略刷新显示
        pin_refresh_mode_t mode = pin_display_decide_refresh_mode();
        if (mode != PIN_REFRESH_NONE) {
            pin_display_refresh(mode);
        }
        
        // 检查是否需要进入睡眠
        if (should_enter_sleep()) {
            pin_enter_deep_sleep(300);  // 睡眠5分钟
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

## 注意事项

1. **刷新频率限制**: 电子墨水屏不宜过于频繁刷新，建议最小间隔30秒
2. **温度影响**: 低温环境下刷新速度会变慢，需要适当调整参数
3. **颜色混叠**: 七色显示可能出现颜色混叠，需要合理选择颜色组合
4. **内存管理**: 显示缓冲区占用较大内存，需要优化内存分配策略
5. **电源管理**: 合理使用深度睡眠，平衡功耗和响应速度