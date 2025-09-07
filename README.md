# Pin | 引脚

[English](#english) | [中文](#中文)

---

## English

**An Open-Source E-Paper Display Platform for Digital Minimalism**

Pin is a hardware and software platform designed to provide essential information display through energy-efficient e-paper technology. Built on the ESP32-C3 microcontroller with a 7-color e-paper display, Pin offers a plugin-based architecture for developers and a distraction-free information experience for users.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.1-blue)](https://github.com/espressif/esp-idf)
[![Build Status](https://img.shields.io/badge/Build-Passing-green)](https://github.com/pin-project/pin)

## Overview

Pin addresses the growing need for mindful technology interaction by providing a low-power, always-on display that presents information without the interruptions common to modern digital devices. The platform serves developers seeking to create ambient information displays and users desiring a more intentional relationship with digital information.

### Key Capabilities

- **Ultra-Low Power Operation**: E-paper display technology enabling weeks of operation on a single charge
- **Wireless Configuration**: Browser-based setup and management via Progressive Web Application
- **Extensible Architecture**: Plugin system allowing custom functionality development
- **Professional-Grade Hardware**: ESP32-C3 RISC-V processor with comprehensive peripheral support
- **Open Development Platform**: Complete hardware and software designs under MIT license

## Technical Architecture

### Hardware Platform

**Core Processing Unit**
- Microcontroller: ESP32-C3 (160MHz RISC-V single-core)
- Memory: 400KB SRAM, 4MB Flash storage
- Connectivity: 802.11 b/g/n WiFi, Bluetooth 5.0 LE
- Power Management: Integrated battery monitoring and deep sleep capabilities

**Display System**
- Panel: FPC-A005 2.9-inch color e-paper display
- Resolution: 600×448 pixels
- Color Depth: 7 colors (Black, White, Red, Yellow, Blue, Green, Orange)
- Interface: 4-wire SPI with additional control signals
- Power Consumption: <1mA standby, ~100mA during refresh

**Expansion Interfaces**
- GPIO: Multiple configurable digital I/O pins
- I2C: Inter-integrated circuit bus for sensor connectivity
- SPI: Serial peripheral interface for additional devices
- ADC: Analog-to-digital converter for sensor inputs

### Software Framework

**Operating System Layer**
- Real-Time OS: FreeRTOS with ESP-IDF framework
- Task Management: Multi-threaded execution with priority scheduling
- Memory Management: Dynamic allocation with heap monitoring
- Power Management: Automatic sleep state transitions

**Application Framework**
- Display Driver: Custom FPC-A005 driver with optimized refresh algorithms
- Plugin System: Sandboxed execution environment with resource monitoring
- Network Stack: HTTP server with RESTful API endpoints
- Configuration Management: Non-volatile storage with backup and recovery

**User Interface**
- Configuration Portal: Progressive Web Application with offline capability
- Device Management: Browser-based interface supporting all major platforms
- API Access: RESTful endpoints for programmatic device control

## Plugin Architecture

The Pin platform implements a secure plugin architecture enabling third-party functionality while maintaining system stability and security.

### Plugin Specifications

- **Language**: C programming language with standardized API
- **Resource Isolation**: Memory limits, execution time constraints, and API rate limiting
- **Configuration**: Per-plugin settings with validation and persistence
- **Lifecycle Management**: Standardized initialization, execution, and cleanup phases

### Development Framework

```c
typedef struct {
    pin_plugin_metadata_t metadata;
    pin_plugin_config_t config;
    esp_err_t (*init)(pin_plugin_context_t* ctx);
    esp_err_t (*update)(pin_plugin_context_t* ctx);
    esp_err_t (*cleanup)(pin_plugin_context_t* ctx);
} pin_plugin_t;
```

### Reference Implementations

- **Digital Clock**: Configurable time display with multiple format options
- **Weather Display**: Network-based weather information with API integration
- **System Monitor**: Device status and performance metrics

## Installation and Configuration

### Development Environment

**Prerequisites**
- ESP-IDF v5.1 or later installation
- Python 3.8+ with pip package manager
- Git version control system
- Serial communication interface (USB-C)

**Environment Setup**
```bash
# Clone repository
git clone https://github.com/pin-project/pin.git
cd pin

# Verify environment (ESP-IDF, toolchain)
bash tools/check_env.sh

# Configure build target
cd firmware && idf.py set-target esp32c3

# Build and deploy
make all && idf.py flash

# Optional: flash PWA assets to SPIFFS (uses firmware/web)
make flash-web
```

### Device Configuration

**Initial Setup Process**
1. Connect to device access point (SSID: Pin-Device-XXXX)
2. Navigate to configuration portal (http://192.168.4.1)
3. Configure network connectivity and device preferences
4. Install and configure desired plugins
5. Complete setup and begin normal operation

**Management Interface**
- Device status monitoring and diagnostics
- Network configuration and connectivity management
- Plugin installation, configuration, and lifecycle control
- System maintenance including updates and factory reset

## API Documentation

### Device Status Endpoint
```
GET /api/status
```
Returns comprehensive device information including hardware status, connectivity state, power levels, and plugin status.

### Plugin Management
```
GET /api/plugins
POST /api/plugins/{plugin_id}
```
Provides plugin enumeration and control functionality for installation, configuration, and lifecycle management.

### Network Configuration
```
GET /api/wifi/scan
POST /api/wifi/connect
```
Enables network discovery and connection management for device connectivity.

## Hardware Integration

### PCB Design Files
Complete hardware design documentation including:
- Electrical schematics with component specifications
- PCB layout files compatible with standard manufacturing processes
- Bill of materials with supplier information and part numbers
- Assembly drawings and manufacturing notes

### Display Variants

If your panel FPC silk reads "FPC-A005 20.06.15" and exposes a 24‑pin FPC without a driver board, it most likely matches a 2.9" BW panel (GDEY029T94, 128×296) with SSD1680 controller, not the 7‑color panel used by this repo's default firmware.

- Identification: See LilyGo GxEPD2 example referencing this exact FPC marking and model mapping:
  - Driver class: GxEPD2_290_GDEY029T94 (SSD1680)
  - Source: https://github.com/Xinyuan-LilyGO/LilyGo-T5-Epaper-Series/blob/d69cfb46554ce8ffe174d7495917f466d7676670/lib/GxEPD2/examples/GxEPD2_WS_ESP32_Driver/GxEPD2_WS_ESP32_Driver.ino#L84

#### What to prepare (BW panel, 24‑pin FPC)
- Driver board: Waveshare ESP32 e‑Paper Driver Board, or Good Display DESPI‑C02 (24‑pin FFC socket).
- Cable: 24‑pin 0.5 mm FFC cable (50–100 mm recommended).
- Wires: Female‑female Dupont jumpers.

#### Wiring (to Waveshare ESP32 e‑Paper Driver Board)
- BUSY → GPIO25
- RST → GPIO26
- DC → GPIO27
- CS → GPIO15
- CLK/SCK → GPIO13
- DIN/MOSI → GPIO14
- 3V3 / GND → 3.3 V / GND
- Notes: This board remaps SPI (HSPI; SCK/MOSI swapped). The example shows this mapping explicitly: lines 20 and 25 in the file above.

#### Firmware support status
- Current firmware targets a 7‑color FPC‑A005 display (600×448). An SSD1680 (BW) driver will be added as `epd_ssd1680` and integrated via `pin_display` selection.
- For immediate bring‑up on the BW panel, you can prototype with GxEPD2 using `GxEPD2_290_GDEY029T94` and the Waveshare board mapping, then migrate to this firmware when the SSD1680 driver lands.

### Pin Assignment
```
Display Interface:     Power Management:     User Interface:
SCK:  GPIO2           Battery ADC: GPIO0    Reset Button: GPIO9
MOSI: GPIO3           Charge Status: GPIO1  Status LED: GPIO8
CS:   GPIO10          
DC:   GPIO4           Expansion Header:
RST:  GPIO5           I2C SDA: GPIO6
BUSY: GPIO7           I2C SCL: GPIO5
```

## Development Guidelines

### Code Standards
- **Language Compliance**: C99 standard with GCC compiler
- **Naming Conventions**: `pin_module_function()` pattern for all public APIs
- **Documentation**: Doxygen-compatible comments for all public interfaces
- **Error Handling**: Comprehensive error checking with standardized return codes

### Testing Framework
- **Unit Testing**: Component-level verification with automated test suites
- **Integration Testing**: Full system validation using Python test framework
- **Hardware-in-Loop**: Automated testing with actual hardware devices
- **Performance Validation**: Power consumption and timing analysis

### Quality Assurance
```bash
# Execute comprehensive test suite
python3 tools/test_system.py --comprehensive

# Perform static code analysis
make lint

# Generate documentation
make docs
```

## Contributing

Pin development follows established open-source practices. Contributions are welcomed through the standard GitHub workflow:

1. **Fork Repository**: Create personal copy of project repository
2. **Development Branch**: Create feature branch for modifications
3. **Implementation**: Develop changes following project coding standards
4. **Testing**: Verify functionality using provided test frameworks
5. **Pull Request**: Submit changes for review and integration

### Contribution Areas
- Core platform enhancements and optimization
- Plugin development and ecosystem expansion
- Hardware design improvements and variants
- Documentation and example development
- Testing framework and quality assurance tools

## Project Governance

**Licensing**: MIT License enabling both personal and commercial use
**Maintainers**: Active community of hardware and software developers
**Support**: Community-driven support through GitHub issues and discussions
**Roadmap**: Feature development guided by community needs and technical feasibility

## Documentation

Comprehensive technical documentation available at:
- `/docs/architecture` - System design and technical specifications  
- `/docs/hardware` - PCB design files and assembly instructions
- `/docs/software` - API reference and development guides
- `/docs/plugins` - Plugin development framework and examples

---

**Pin Project** - *Enabling thoughtful interaction with digital information*

For technical support, feature requests, or community discussion, please visit our [GitHub repository](https://github.com/pin-project/pin).

---

## 中文

**开源电子墨水屏显示平台，专注数字极简主义**

Pin 是一个硬件和软件平台，旨在通过节能的电子纸技术提供必要的信息显示。基于 ESP32-C3 微控制器和 7 色电子墨水屏构建，Pin 为开发者提供了基于插件的架构，为用户提供了无干扰的信息体验。

[![许可证: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.1-blue)](https://github.com/espressif/esp-idf)
[![构建状态](https://img.shields.io/badge/Build-Passing-green)](https://github.com/pin-project/pin)

### 概述

Pin 通过提供低功耗、始终显示的屏幕来满足对有意识技术交互日益增长的需求，该屏幕在不受现代数字设备常见干扰的情况下呈现信息。该平台服务于寻求创建环境信息显示的开发者，以及渴望与数字信息建立更有意义关系的用户。

#### 核心功能

- **超低功耗运行**: 电子纸显示技术，单次充电可运行数周
- **无线配置**: 通过渐进式网页应用进行基于浏览器的设置和管理
- **可扩展架构**: 插件系统允许自定义功能开发
- **专业级硬件**: ESP32-C3 RISC-V 处理器，具有全面的外设支持
- **开放开发平台**: MIT 许可证下的完整硬件和软件设计

### 技术架构

#### 硬件平台

**核心处理单元**
- 微控制器: ESP32-C3 (160MHz RISC-V 单核)
- 内存: 400KB SRAM, 4MB Flash 存储
- 连接性: 802.11 b/g/n WiFi, 蓝牙 5.0 LE
- 电源管理: 集成电池监控和深度睡眠功能

**显示系统**
- 面板: FPC-A005 2.9英寸彩色电子纸显示屏
- 分辨率: 600×448 像素
- 色深: 7种颜色（黑、白、红、黄、蓝、绿、橙）
- 接口: 4线 SPI 带额外控制信号
- 功耗: <1mA 待机，刷新时约100mA

#### 软件框架

**操作系统层**
- 实时操作系统: FreeRTOS 与 ESP-IDF 框架
- 任务管理: 优先级调度的多线程执行
- 内存管理: 带堆监控的动态分配
- 电源管理: 自动睡眠状态转换

**应用框架**
- 显示驱动: 自定义 FPC-A005 驱动，优化刷新算法
- 插件系统: 沙盒执行环境，资源监控
- 网络栈: 带 RESTful API 端点的 HTTP 服务器
- 配置管理: 带备份和恢复的非易失性存储

### 开发环境

**先决条件**
- ESP-IDF v5.1 或更新版本安装
- Python 3.8+ 和 pip 包管理器
- Git 版本控制系统
- 串口通信接口 (USB-C)

**环境设置**
```bash
# 克隆仓库
git clone https://github.com/pin-project/pin.git
cd pin

# 配置构建环境
cd firmware
idf.py set-target esp32c3

# 构建和部署
make all
idf.py flash
make flash-web
```

### 设备配置

**初始设置流程**
1. 连接到设备接入点 (SSID: Pin-Device-XXXX)
2. 导航到配置门户 (http://192.168.4.1)
3. 配置网络连接和设备首选项
4. 安装和配置所需插件
5. 完成设置并开始正常运行

### 贡献

Pin 开发遵循既定的开源实践。欢迎通过标准 GitHub 工作流程贡献：

1. **fork 仓库**: 创建项目仓库的个人副本
2. **开发分支**: 为修改创建功能分支
3. **实现**: 按照项目编码标准开发更改
4. **测试**: 使用提供的测试框架验证功能
5. **拉取请求**: 提交更改以供审查和集成

### 项目治理

**许可证**: MIT 许可证，允许个人和商业使用
**维护者**: 硬件和软件开发者的活跃社区
**支持**: 通过 GitHub issues 和讨论的社区驱动支持
**路线图**: 由社区需求和技术可行性指导的功能开发

### 文档

完整技术文档可在以下位置获得：
- `/docs/architecture` - 系统设计和技术规范  
- `/docs/hardware` - PCB 设计文件和组装说明
- `/docs/software` - API 参考和开发指南
- `/docs/plugins` - 插件开发框架和示例

---

**Pin 项目** - *促进与数字信息的有意识互动*

如需技术支持、功能请求或社区讨论，请访问我们的 [GitHub 仓库](https://github.com/pin-project/pin)。
