# Pin 开发入门指南

## 概述

本指南将帮助您快速上手 Pin 项目的开发工作，包括环境搭建、代码结构理解、以及如何进行各个模块的开发。

## 开发环境搭建

### 1. 系统要求

**支持的操作系统：**
- Ubuntu 18.04 LTS 或更高版本
- macOS 10.15 (Catalina) 或更高版本  
- Windows 10 (通过 WSL2)

**硬件要求：**
- 至少 8GB RAM
- 20GB 可用磁盘空间
- USB 端口用于设备连接

### 2. 安装必需软件

#### 安装 ESP-IDF

```bash
# 1. 安装依赖
sudo apt-get update
sudo apt-get install git wget flex bison gperf python3 python3-pip python3-setuptools cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0

# 2. 下载 ESP-IDF
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git

# 3. 安装工具链
cd esp-idf
./install.sh esp32c3

# 4. 设置环境变量
. ./export.sh

# 5. 验证安装
idf.py --version
```

#### 配置 VS Code (推荐)

1. 安装 VS Code
2. 安装扩展：
   - C/C++
   - ESP-IDF Extension
   - GitLens
   - Bracket Pair Colorizer

3. 配置 ESP-IDF 扩展：
   ```json
   {
       "idf.espIdfPath": "~/esp/esp-idf",
       "idf.pythonBinPath": "~/esp/esp-idf/python_env/idf5.0_py3.8_env/bin/python",
       "idf.toolsPath": "~/.espressif"
   }
   ```

### 3. 克隆项目

```bash
git clone https://github.com/pin-project/pin-device.git
cd pin-device

# 安装 Git hooks (可选但推荐)
pip install pre-commit
pre-commit install
```

## 项目结构详解

### 核心目录结构

```
pin-device/
├── .github/                # GitHub 工作流和模板
├── docs/                   # 项目文档
│   ├── architecture/       # 架构设计文档
│   ├── development/        # 开发指南  
│   └── api/               # API 参考文档
├── firmware/              # ESP32-C3 固件
│   ├── CMakeLists.txt     # 主 CMake 配置
│   ├── sdkconfig          # ESP-IDF 配置文件
│   ├── main/              # 主应用程序
│   ├── components/        # 自定义组件
│   ├── plugins/           # 内置插件
│   └── web/               # PWA 应用
├── hardware/              # 硬件设计文件
├── tools/                 # 开发工具和脚本
├── .gitignore            # Git 忽略文件
├── .pre-commit-config.yaml # 预提交钩子配置
├── LICENSE               # 开源许可证
└── README.md             # 项目说明文档
```

### 固件目录详解

```
firmware/
├── main/                          # 主应用程序
│   ├── CMakeLists.txt            # 主程序 CMake 配置
│   ├── pin_main.c                # 程序入口点
│   ├── pin_display.c/.h          # 显示系统实现
│   ├── pin_wifi.c/.h             # WiFi 配置实现
│   ├── pin_plugin.c/.h           # 插件系统实现  
│   ├── pin_config.c/.h           # 配置管理实现
│   └── pin_power.c/.h            # 电源管理实现
├── components/                    # 自定义组件
│   ├── fpc_a005/                 # FPC-A005 驱动组件
│   │   ├── CMakeLists.txt
│   │   ├── include/fpc_a005.h
│   │   └── src/fpc_a005.c
│   ├── pin_ui/                   # UI 组件库
│   │   ├── CMakeLists.txt
│   │   ├── include/pin_ui.h
│   │   └── src/pin_ui.c
│   ├── pin_plugin_api/           # 插件 API 组件
│   │   ├── CMakeLists.txt
│   │   ├── include/pin_plugin.h
│   │   └── src/pin_plugin_api.c
│   └── pin_webserver/            # Web 服务器组件
│       ├── CMakeLists.txt
│       ├── include/pin_webserver.h
│       └── src/pin_webserver.c
├── plugins/                       # 内置插件
│   ├── clock/                    # 时钟插件
│   ├── weather/                  # 天气插件
│   └── calendar/                 # 日历插件
└── web/                          # PWA 应用文件
    ├── index.html                # 主页面
    ├── app.js                    # 应用逻辑
    ├── sw.js                     # Service Worker
    ├── manifest.json             # PWA 清单
    └── assets/                   # 静态资源
```

## 构建和调试

### 1. 配置项目

```bash
cd firmware

# 设置目标芯片
idf.py set-target esp32c3

# 配置项目 (可选)
idf.py menuconfig
```

### 2. 构建固件

```bash
# 完整构建
idf.py build

# 增量构建
idf.py build

# 清理构建
idf.py fullclean
```

### 3. 烧录和监控

```bash
# 烧录固件
idf.py flash

# 串口监控
idf.py monitor

# 烧录并监控 (常用)
idf.py flash monitor

# 退出监控: Ctrl+]
```

### 4. 调试技巧

#### 日志调试

```c
#include "esp_log.h"

static const char* TAG = "PIN_MAIN";

// 不同级别的日志
ESP_LOGE(TAG, "Error occurred: %s", error_msg);
ESP_LOGW(TAG, "Warning: %s", warning_msg);  
ESP_LOGI(TAG, "Info: %s", info_msg);
ESP_LOGD(TAG, "Debug: %s", debug_msg);
ESP_LOGV(TAG, "Verbose: %s", verbose_msg);
```

#### 配置日志级别

```bash
idf.py menuconfig
# Component config -> Log output -> Default log verbosity
```

#### 使用 GDB 调试

```bash
# 启动 OpenOCD
openocd -f board/esp32c3-builtin.cfg

# 在另一个终端启动 GDB
xtensa-esp32-elf-gdb build/pin-device.elf
(gdb) target remote :3333
(gdb) monitor reset halt
(gdb) break app_main
(gdb) continue
```

#### 性能分析

```bash
# 启用性能分析
idf.py menuconfig
# Component config -> ESP32C3-specific -> CPU frequency -> 160 MHz

# 查看任务状态
vTaskList() // 在代码中调用

# 内存使用分析
esp_get_free_heap_size()
esp_get_minimum_free_heap_size()
```

## 开发工作流程

### 1. 功能开发流程

```bash
# 1. 创建功能分支
git checkout -b feature/new-awesome-feature

# 2. 开发功能
# ... 编码工作 ...

# 3. 运行测试
idf.py build
idf.py flash monitor

# 4. 提交更改
git add .
git commit -m "feat: add awesome new feature

- Implement awesome feature X
- Add unit tests
- Update documentation"

# 5. 推送分支
git push origin feature/new-awesome-feature

# 6. 创建 Pull Request
```

### 2. 代码风格指南

#### C 代码风格

```c
// 文件头注释
/**
 * @file pin_display.c
 * @brief Pin 显示系统实现
 * @author Pin Team
 * @date 2024-01-01
 */

#include "pin_display.h"

// 静态函数声明
static esp_err_t display_init_spi(void);
static esp_err_t display_send_command(uint8_t cmd);

// 全局变量 (避免使用，必要时使用 g_ 前缀)
static pin_display_config_t g_display_config = {0};

// 函数实现
esp_err_t pin_display_init(void) {
    esp_err_t ret = ESP_OK;
    
    // 初始化 SPI
    ret = display_init_spi();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ret;
}

// 静态函数实现
static esp_err_t display_init_spi(void) {
    // SPI 初始化代码
    return ESP_OK;
}
```

#### 命名约定

- **函数**: `pin_module_action()` (小写+下划线)
- **变量**: `variable_name` (小写+下划线)
- **常量**: `PIN_MAX_VALUE` (大写+下划线)
- **类型**: `pin_display_config_t` (小写+下划线+_t后缀)
- **全局变量**: `g_variable_name` (g_前缀)
- **静态变量**: `s_variable_name` (s_前缀)

### 3. 提交消息规范

使用 Conventional Commits 规范：

```
<type>[optional scope]: <description>

[optional body]

[optional footer(s)]
```

类型包括：
- `feat`: 新功能
- `fix`: 错误修复
- `docs`: 文档更新
- `style`: 代码格式化
- `refactor`: 代码重构
- `test`: 添加测试
- `chore`: 构建过程或辅助工具的变动

示例：
```
feat(display): add color management for FPC-A005

- Implement 7-color palette support
- Add color conversion utilities
- Update display refresh logic for color optimization

Closes #123
```

## 组件开发指南

### 1. 创建新组件

```bash
# 在 components 目录下创建新组件
mkdir -p firmware/components/my_component/{include,src}
cd firmware/components/my_component
```

创建 `CMakeLists.txt`：
```cmake
idf_component_register(
    SRCS "src/my_component.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_common
    PRIV_REQUIRES esp_timer
)
```

创建头文件 `include/my_component.h`：
```c
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化组件
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t my_component_init(void);

/**
 * @brief 组件主要功能函数
 * @param param 参数说明
 * @return ESP_OK 成功，其他值表示错误  
 */
esp_err_t my_component_do_something(int param);

#ifdef __cplusplus
}
#endif
```

创建源文件 `src/my_component.c`：
```c
#include "my_component.h"
#include "esp_log.h"

static const char* TAG = "MY_COMPONENT";

esp_err_t my_component_init(void) {
    ESP_LOGI(TAG, "Initializing my component");
    
    // 初始化逻辑
    
    ESP_LOGI(TAG, "My component initialized successfully");
    return ESP_OK;
}

esp_err_t my_component_do_something(int param) {
    ESP_LOGD(TAG, "Doing something with param: %d", param);
    
    // 功能实现
    
    return ESP_OK;
}
```

### 2. 插件开发详解

#### 插件结构

```c
// plugins/my_plugin/my_plugin.c
#include "pin_plugin.h"

// 插件私有数据结构
typedef struct {
    int counter;
    char display_text[64];
    bool enabled;
} my_plugin_data_t;

// 插件生命周期函数
static esp_err_t my_plugin_init(pin_plugin_context_t* ctx);
static esp_err_t my_plugin_start(pin_plugin_context_t* ctx);
static esp_err_t my_plugin_update(pin_plugin_context_t* ctx);
static esp_err_t my_plugin_render(pin_plugin_context_t* ctx, pin_widget_region_t* region);
static esp_err_t my_plugin_config_changed(pin_plugin_context_t* ctx, const char* key, const char* value);
static esp_err_t my_plugin_stop(pin_plugin_context_t* ctx);
static esp_err_t my_plugin_cleanup(pin_plugin_context_t* ctx);

// 插件定义
pin_plugin_t my_plugin = {
    .metadata = {
        .name = "my_plugin",
        .version = "1.0.0",
        .author = "Your Name",
        .description = "My awesome plugin",
        .homepage = "https://github.com/yourname/my-plugin"
    },
    .config = {
        .memory_limit = 8192,        // 8KB
        .update_interval = 30,       // 30秒
        .api_rate_limit = 10,        // 10次/分钟
        .auto_start = true,
        .persistent = true
    },
    .init = my_plugin_init,
    .start = my_plugin_start,
    .update = my_plugin_update,
    .render = my_plugin_render,
    .config_changed = my_plugin_config_changed,
    .stop = my_plugin_stop,
    .cleanup = my_plugin_cleanup,
    .enabled = false,
    .initialized = false,
    .running = false,
    .private_data = NULL
};

// 插件实现
static esp_err_t my_plugin_init(pin_plugin_context_t* ctx) {
    // 分配私有数据
    my_plugin_data_t* data = pin_plugin_malloc(ctx, sizeof(my_plugin_data_t));
    if (!data) {
        return ESP_ERR_NO_MEM;
    }
    
    // 初始化数据
    data->counter = 0;
    strcpy(data->display_text, "Hello Plugin!");
    data->enabled = true;
    
    ctx->plugin->private_data = data;
    
    // 从配置加载设置
    char config_value[64];
    if (ctx->api.config_get("display_text", config_value, sizeof(config_value)) == ESP_OK) {
        strncpy(data->display_text, config_value, sizeof(data->display_text) - 1);
    }
    
    ctx->api.log_info("MY_PLUGIN", "Plugin initialized");
    return ESP_OK;
}

static esp_err_t my_plugin_update(pin_plugin_context_t* ctx) {
    my_plugin_data_t* data = (my_plugin_data_t*)ctx->plugin->private_data;
    
    if (!data->enabled) {
        return ESP_OK;
    }
    
    // 更新计数器
    data->counter++;
    
    // 更新显示内容
    char content[64];
    snprintf(content, sizeof(content), "%s (%d)", data->display_text, data->counter);
    
    ctx->api.display_update_content(content);
    
    ctx->api.log_debug("MY_PLUGIN", "Updated counter to %d", data->counter);
    return ESP_OK;
}
```

#### 注册插件

```c
// 在 main/pin_main.c 中注册插件
#include "my_plugin.h"

void app_main(void) {
    // 其他初始化代码...
    
    // 注册插件
    pin_plugin_register(&my_plugin);
    
    // 启用插件
    pin_plugin_enable("my_plugin", true);
}
```

### 3. PWA 组件开发

#### 创建新的配置组件

```javascript
// web/components/my-config.js
class MyConfigComponent {
    constructor(api, app) {
        this.api = api;
        this.app = app;
        this.container = null;
        
        this.init();
    }
    
    init() {
        this.createUI();
        this.bindEvents();
        this.loadData();
    }
    
    createUI() {
        this.container = document.createElement('div');
        this.container.className = 'card';
        this.container.innerHTML = `
            <h2 class="card-title">My Configuration</h2>
            <div class="form-group">
                <label for="my-input">设置值</label>
                <input type="text" id="my-input" class="form-control" placeholder="输入值">
            </div>
            <div class="form-group">
                <label>
                    <input type="checkbox" id="my-checkbox"> 启用功能
                </label>
            </div>
            <button id="my-save-btn" class="btn btn-primary">保存设置</button>
        `;
        
        // 添加到页面
        const container = document.getElementById('config-container');
        container.appendChild(this.container);
    }
    
    bindEvents() {
        const saveBtn = this.container.querySelector('#my-save-btn');
        saveBtn.addEventListener('click', this.handleSave.bind(this));
        
        const checkbox = this.container.querySelector('#my-checkbox');
        checkbox.addEventListener('change', this.handleToggle.bind(this));
    }
    
    async loadData() {
        try {
            const config = await this.api.request('/api/my-plugin/config');
            
            // 更新 UI
            document.getElementById('my-input').value = config.display_text || '';
            document.getElementById('my-checkbox').checked = config.enabled || false;
            
        } catch (error) {
            console.error('Failed to load configuration:', error);
            this.app.showToast('加载配置失败', 'error');
        }
    }
    
    async handleSave() {
        const inputValue = document.getElementById('my-input').value;
        const checkboxValue = document.getElementById('my-checkbox').checked;
        
        try {
            await this.api.request('/api/my-plugin/config', {
                method: 'POST',
                body: JSON.stringify({
                    display_text: inputValue,
                    enabled: checkboxValue
                })
            });
            
            this.app.showToast('设置已保存', 'success');
        } catch (error) {
            console.error('Failed to save configuration:', error);  
            this.app.showToast('保存设置失败', 'error');
        }
    }
    
    handleToggle(event) {
        const enabled = event.target.checked;
        
        // 立即更新插件状态
        this.api.request('/api/my-plugin/toggle', {
            method: 'POST',
            body: JSON.stringify({ enabled })
        }).catch(error => {
            console.error('Failed to toggle plugin:', error);
        });
    }
}

// 导出组件
window.MyConfigComponent = MyConfigComponent;
```

## 测试指南

### 1. 单元测试

创建测试文件 `test/test_my_component.c`：

```c
#include "unity.h"
#include "my_component.h"

void setUp(void) {
    // 每个测试前的设置
}

void tearDown(void) {
    // 每个测试后的清理
}

void test_my_component_init(void) {
    esp_err_t ret = my_component_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

void test_my_component_do_something(void) {
    int param = 42;
    esp_err_t ret = my_component_do_something(param);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

void app_main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_my_component_init);
    RUN_TEST(test_my_component_do_something);
    
    UNITY_END();
}
```

运行测试：
```bash
idf.py build flash monitor
```

### 2. 集成测试

```bash
# 创建集成测试配置
cp sdkconfig sdkconfig.test

# 修改测试配置
idf.py -D SDKCONFIG=sdkconfig.test menuconfig

# 构建和运行测试
idf.py -D SDKCONFIG=sdkconfig.test build flash monitor
```

### 3. 硬件在环测试

```python
# test/hardware_test.py
import serial
import time
import json

def test_display_refresh():
    """测试显示刷新功能"""
    ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=10)
    
    # 发送测试命令
    test_cmd = {"action": "display_test", "pattern": "checkerboard"}
    ser.write(json.dumps(test_cmd).encode() + b'\n')
    
    # 等待响应
    response = ser.readline().decode().strip()
    result = json.loads(response)
    
    assert result['status'] == 'success'
    ser.close()

if __name__ == '__main__':
    test_display_refresh()
    print("Hardware test passed!")
```

## 常见问题解决

### 1. 编译错误

**错误**: `fatal error: 'my_header.h' file not found`
**解决**: 检查 CMakeLists.txt 中的 INCLUDE_DIRS 设置

**错误**: `undefined reference to 'my_function'`
**解决**: 检查函数声明和定义，确保链接了正确的库

### 2. 运行时错误

**错误**: `Guru Meditation Error: Core 0 panic'ed (LoadProhibited)`
**解决**: 检查指针使用，可能访问了空指针或无效内存

**错误**: `Task watchdog got triggered`
**解决**: 检查是否有长时间阻塞的代码，添加 `vTaskDelay()` 或增加看门狗超时

### 3. WiFi 连接问题

```c
// 调试 WiFi 连接
void wifi_event_handler(void* arg, esp_event_base_t event_base,
                       int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi started, connecting...");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WiFi connected");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGW(TAG, "WiFi disconnected, retrying...");
                esp_wifi_connect();
                break;
        }
    }
}
```

### 4. 显示器问题

```c
// 检查 SPI 通信
void debug_spi_communication(void) {
    // 检查引脚配置
    ESP_LOGI(TAG, "DC pin: %d", PIN_DC);
    ESP_LOGI(TAG, "RST pin: %d", PIN_RST);
    ESP_LOGI(TAG, "BUSY pin: %d", PIN_BUSY);
    
    // 检查 BUSY 状态
    int busy_state = gpio_get_level(PIN_BUSY);
    ESP_LOGI(TAG, "BUSY pin state: %d", busy_state);
    
    // 发送测试命令
    display_send_command(0x00);  // NOP command
}
```

## 性能优化

### 1. 内存优化

```c
// 使用内存池
#define MEMORY_POOL_SIZE 4096
static uint8_t memory_pool[MEMORY_POOL_SIZE];
static size_t pool_offset = 0;

void* pool_malloc(size_t size) {
    if (pool_offset + size > MEMORY_POOL_SIZE) {
        return NULL;
    }
    void* ptr = &memory_pool[pool_offset];
    pool_offset += size;
    return ptr;
}

// 监控内存使用
void print_memory_info(void) {
    ESP_LOGI(TAG, "Free heap: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Min free heap: %d bytes", esp_get_minimum_free_heap_size());
    ESP_LOGI(TAG, "Largest free block: %d bytes", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
}
```

### 2. 功耗优化

```c
// 使用深度睡眠
void enter_deep_sleep(uint32_t sleep_time_sec) {
    ESP_LOGI(TAG, "Entering deep sleep for %d seconds", sleep_time_sec);
    
    // 配置唤醒源
    esp_sleep_enable_timer_wakeup(sleep_time_sec * 1000000ULL);
    esp_sleep_enable_ext0_wakeup(GPIO_WAKE_PIN, 0);
    
    // 进入深度睡眠
    esp_deep_sleep_start();
}

// 动态频率调节
void adjust_cpu_frequency(bool high_performance) {
    if (high_performance) {
        esp_pm_configure(&(esp_pm_config_esp32c3_t){
            .max_freq_mhz = 160,
            .min_freq_mhz = 80
        });
    } else {
        esp_pm_configure(&(esp_pm_config_esp32c3_t){
            .max_freq_mhz = 80,
            .min_freq_mhz = 10
        });
    }
}
```

## 下一步

现在您已经掌握了 Pin 项目的基本开发知识，可以：

1. **尝试修改现有插件**：从时钟插件开始，修改显示格式或添加新功能
2. **创建您的第一个插件**：实现一个简单的计数器或温度显示插件
3. **优化 PWA 界面**：添加新的配置页面或改进用户体验
4. **贡献代码**：提交您的改进到项目仓库

更多详细信息请参考：
- [插件开发指南](plugin-development.md)
- [PWA 开发指南](pwa-development.md)  
- [API 参考文档](../api/)
- [架构设计文档](../architecture/)

如有问题，欢迎在 GitHub Issues 中提出或加入我们的社区讨论。