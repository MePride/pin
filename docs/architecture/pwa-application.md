# Pin PWA 配置应用设计文档

## 概述

Pin PWA (Progressive Web App) 是内置在ESP32-C3设备中的配置界面，通过Web技术实现跨平台的设备管理功能。用户无需安装APP，通过浏览器即可完成设备配置。

## 设计原则

1. **零安装**: 无需下载APP，浏览器直接访问
2. **响应式设计**: 兼容手机、平板、桌面端
3. **离线支持**: Service Worker提供离线缓存
4. **轻量级**: 针对ESP32资源限制优化
5. **用户友好**: 简洁直观的操作界面

## 技术架构

### 技术栈选择

```
PWA技术栈
├── 前端技术
│   ├── HTML5 (语义化标记)
│   ├── CSS3 (响应式布局 + CSS Grid/Flexbox)
│   ├── Vanilla JavaScript (避免框架依赖)
│   └── Web APIs (Service Worker, Cache API, Fetch API)
├── 通信协议
│   ├── HTTP/HTTPS (RESTful API)
│   ├── WebSocket (实时通信)
│   └── Server-Sent Events (状态推送)
├── 存储方案
│   ├── localStorage (本地配置缓存)
│   ├── IndexedDB (离线数据存储)
│   └── Cache API (资源缓存)
└── 构建工具
    ├── 内联CSS/JS (减少HTTP请求)
    ├── 资源压缩 (gzip)
    └── 图片优化 (WebP/SVG)
```

### 文件结构

```
firmware/web/
├── index.html              # 主页面
├── manifest.json          # PWA清单文件
├── sw.js                  # Service Worker
├── app.js                 # 主应用逻辑
├── styles.css             # 样式文件
├── assets/                # 静态资源
│   ├── icons/            # 应用图标
│   ├── fonts/            # 字体文件
│   └── images/           # 图片资源
└── components/           # 组件模块
    ├── wifi-config.js    # WiFi配置组件
    ├── plugin-manager.js # 插件管理组件
    ├── display-settings.js # 显示设置组件
    └── device-status.js  # 设备状态组件
```

## 核心组件设计

### 主应用结构

```html
<!-- index.html -->
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
    <title>Pin 设备配置</title>
    <link rel="manifest" href="/manifest.json">
    <link rel="icon" href="/assets/icons/favicon.ico">
    <meta name="theme-color" content="#2196F3">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <meta name="apple-mobile-web-app-status-bar-style" content="default">
    <meta name="apple-mobile-web-app-title" content="Pin Config">
    
    <!-- 内联关键CSS以提高加载速度 -->
    <style>
        :root {
            --primary-color: #2196F3;
            --secondary-color: #FFC107;
            --success-color: #4CAF50;
            --error-color: #F44336;
            --warning-color: #FF9800;
            --text-primary: #212121;
            --text-secondary: #757575;
            --background: #FAFAFA;
            --surface: #FFFFFF;
            --border: #E0E0E0;
        }
        
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }
        
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background-color: var(--background);
            color: var(--text-primary);
            line-height: 1.5;
            -webkit-font-smoothing: antialiased;
        }
        
        .container {
            max-width: 428px;
            margin: 0 auto;
            padding: 16px;
            min-height: 100vh;
        }
        
        .header {
            text-align: center;
            margin-bottom: 24px;
            padding: 16px 0;
        }
        
        .logo {
            width: 64px;
            height: 64px;
            margin: 0 auto 16px;
            background: var(--primary-color);
            border-radius: 16px;
            display: flex;
            align-items: center;
            justify-content: center;
            color: white;
            font-size: 24px;
            font-weight: bold;
        }
        
        .card {
            background: var(--surface);
            border-radius: 12px;
            padding: 20px;
            margin-bottom: 16px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.1);
            border: 1px solid var(--border);
        }
        
        .card-title {
            font-size: 18px;
            font-weight: 600;
            margin-bottom: 16px;
            color: var(--text-primary);
        }
        
        .form-group {
            margin-bottom: 16px;
        }
        
        label {
            display: block;
            margin-bottom: 8px;
            font-weight: 500;
            color: var(--text-primary);
        }
        
        input, select, textarea {
            width: 100%;
            padding: 12px;
            border: 2px solid var(--border);
            border-radius: 8px;
            font-size: 16px;
            transition: border-color 0.2s;
        }
        
        input:focus, select:focus, textarea:focus {
            outline: none;
            border-color: var(--primary-color);
        }
        
        .btn {
            display: inline-block;
            padding: 12px 24px;
            background: var(--primary-color);
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            font-weight: 500;
            cursor: pointer;
            text-decoration: none;
            text-align: center;
            transition: all 0.2s;
            width: 100%;
        }
        
        .btn:hover {
            background: #1976D2;
            transform: translateY(-1px);
        }
        
        .btn:active {
            transform: translateY(0);
        }
        
        .btn-secondary {
            background: var(--border);
            color: var(--text-primary);
        }
        
        .btn-success {
            background: var(--success-color);
        }
        
        .btn-danger {
            background: var(--error-color);
        }
        
        .status-indicator {
            display: inline-block;
            width: 8px;
            height: 8px;
            border-radius: 50%;
            margin-right: 8px;
        }
        
        .status-connected {
            background: var(--success-color);
        }
        
        .status-disconnected {
            background: var(--error-color);
        }
        
        .status-connecting {
            background: var(--warning-color);
            animation: pulse 1.5s infinite;
        }
        
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }
        
        .loading {
            display: inline-block;
            width: 20px;
            height: 20px;
            border: 2px solid #f3f3f3;
            border-top: 2px solid var(--primary-color);
            border-radius: 50%;
            animation: spin 1s linear infinite;
        }
        
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
        
        .hidden {
            display: none !important;
        }
        
        .toast {
            position: fixed;
            top: 20px;
            right: 20px;
            left: 20px;
            max-width: 400px;
            margin: 0 auto;
            padding: 16px;
            border-radius: 8px;
            color: white;
            font-weight: 500;
            z-index: 1000;
            transform: translateY(-100px);
            transition: transform 0.3s;
        }
        
        .toast.show {
            transform: translateY(0);
        }
        
        .toast-success {
            background: var(--success-color);
        }
        
        .toast-error {
            background: var(--error-color);
        }
        
        .toast-warning {
            background: var(--warning-color);
        }
        
        .plugin-item {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 12px 0;
            border-bottom: 1px solid var(--border);
        }
        
        .plugin-item:last-child {
            border-bottom: none;
        }
        
        .plugin-info h4 {
            margin-bottom: 4px;
            color: var(--text-primary);
        }
        
        .plugin-info p {
            color: var(--text-secondary);
            font-size: 14px;
        }
        
        .switch {
            position: relative;
            display: inline-block;
            width: 48px;
            height: 28px;
        }
        
        .switch input {
            opacity: 0;
            width: 0;
            height: 0;
        }
        
        .slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: #ccc;
            transition: .4s;
            border-radius: 28px;
        }
        
        .slider:before {
            position: absolute;
            content: "";
            height: 20px;
            width: 20px;
            left: 4px;
            bottom: 4px;
            background-color: white;
            transition: .4s;
            border-radius: 50%;
        }
        
        input:checked + .slider {
            background-color: var(--primary-color);
        }
        
        input:checked + .slider:before {
            transform: translateX(20px);
        }
        
        /* 响应式设计 */
        @media (min-width: 768px) {
            .container {
                max-width: 600px;
                padding: 24px;
            }
            
            .header {
                margin-bottom: 32px;
            }
            
            .card {
                padding: 24px;
                margin-bottom: 20px;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <!-- 应用头部 -->
        <div class="header">
            <div class="logo">P</div>
            <h1>Pin 设备配置</h1>
            <div id="connection-status" class="connection-status">
                <span class="status-indicator status-connecting"></span>
                <span>连接中...</span>
            </div>
        </div>
        
        <!-- 设备状态卡片 -->
        <div class="card" id="device-status-card">
            <h2 class="card-title">设备状态</h2>
            <div id="device-info">
                <div class="loading"></div>
                <span>正在加载设备信息...</span>
            </div>
        </div>
        
        <!-- WiFi配置卡片 -->
        <div class="card" id="wifi-config-card">
            <h2 class="card-title">WiFi 配置</h2>
            <form id="wifi-form">
                <div class="form-group">
                    <label for="wifi-ssid">网络名称 (SSID)</label>
                    <select id="wifi-ssid" required>
                        <option value="">选择WiFi网络...</option>
                    </select>
                </div>
                <div class="form-group">
                    <label for="wifi-password">密码</label>
                    <input type="password" id="wifi-password" placeholder="输入WiFi密码">
                </div>
                <button type="submit" class="btn" id="wifi-connect-btn">
                    <span>连接 WiFi</span>
                </button>
            </form>
            <button type="button" class="btn btn-secondary" id="wifi-scan-btn" style="margin-top: 8px;">
                <span>扫描网络</span>
            </button>
        </div>
        
        <!-- 插件管理卡片 -->
        <div class="card" id="plugin-manager-card">
            <h2 class="card-title">插件管理</h2>
            <div id="plugin-list">
                <div class="loading"></div>
                <span>正在加载插件列表...</span>
            </div>
        </div>
        
        <!-- 显示设置卡片 -->
        <div class="card" id="display-settings-card">
            <h2 class="card-title">显示设置</h2>
            <div class="form-group">
                <label for="refresh-interval">刷新间隔</label>
                <select id="refresh-interval">
                    <option value="30">30秒</option>
                    <option value="60" selected>1分钟</option>
                    <option value="300">5分钟</option>
                    <option value="600">10分钟</option>
                    <option value="1800">30分钟</option>
                </select>
            </div>
            <div class="form-group">
                <label>
                    <input type="checkbox" id="power-save-mode" checked>
                    省电模式
                </label>
            </div>
            <div class="form-group">
                <label>
                    <input type="checkbox" id="auto-brightness">
                    自动亮度
                </label>
            </div>
            <button type="button" class="btn" id="apply-display-settings">
                应用设置
            </button>
        </div>
        
        <!-- 系统设置卡片 -->
        <div class="card" id="system-settings-card">
            <h2 class="card-title">系统设置</h2>
            <button type="button" class="btn btn-secondary" id="restart-device" style="margin-bottom: 8px;">
                重启设备
            </button>
            <button type="button" class="btn btn-secondary" id="factory-reset" style="margin-bottom: 8px;">
                恢复出厂设置
            </button>
            <button type="button" class="btn btn-secondary" id="check-update">
                检查更新
            </button>
        </div>
    </div>
    
    <!-- Toast 通知容器 -->
    <div id="toast-container"></div>
    
    <!-- 加载应用脚本 -->
    <script src="/app.js"></script>
</body>
</html>
```

### PWA 清单文件

```json
{
    "name": "Pin 设备配置",
    "short_name": "Pin Config",
    "description": "Pin 电子墨水屏设备配置应用",
    "start_url": "/",
    "scope": "/",
    "display": "standalone",
    "orientation": "portrait-primary",
    "theme_color": "#2196F3",
    "background_color": "#FFFFFF",
    "lang": "zh-CN",
    "dir": "ltr",
    "categories": ["utilities", "productivity"],
    "icons": [
        {
            "src": "/assets/icons/icon-72.png",
            "sizes": "72x72",
            "type": "image/png",
            "purpose": "any"
        },
        {
            "src": "/assets/icons/icon-96.png",
            "sizes": "96x96",
            "type": "image/png",
            "purpose": "any"
        },
        {
            "src": "/assets/icons/icon-128.png",
            "sizes": "128x128",
            "type": "image/png",
            "purpose": "any"
        },
        {
            "src": "/assets/icons/icon-144.png",
            "sizes": "144x144",
            "type": "image/png",
            "purpose": "any"
        },
        {
            "src": "/assets/icons/icon-152.png",
            "sizes": "152x152",
            "type": "image/png",
            "purpose": "any"
        },
        {
            "src": "/assets/icons/icon-192.png",
            "sizes": "192x192",
            "type": "image/png",
            "purpose": "any maskable"
        },
        {
            "src": "/assets/icons/icon-384.png",
            "sizes": "384x384",
            "type": "image/png",
            "purpose": "any"
        },
        {
            "src": "/assets/icons/icon-512.png",
            "sizes": "512x512",
            "type": "image/png",
            "purpose": "any maskable"
        }
    ],
    "screenshots": [
        {
            "src": "/assets/screenshots/mobile-1.png",
            "sizes": "390x844",
            "type": "image/png",
            "form_factor": "narrow",
            "label": "Pin 配置主界面"
        }
    ],
    "shortcuts": [
        {
            "name": "WiFi设置",
            "short_name": "WiFi",
            "url": "/#wifi",
            "icons": [
                {
                    "src": "/assets/icons/wifi-96.png",
                    "sizes": "96x96"
                }
            ]
        },
        {
            "name": "插件管理",
            "short_name": "插件",
            "url": "/#plugins",
            "icons": [
                {
                    "src": "/assets/icons/plugin-96.png",
                    "sizes": "96x96"
                }
            ]
        }
    ]
}
```

### 主应用逻辑

```javascript
// app.js - 主应用逻辑
class PinConfigApp {
    constructor() {
        this.deviceAPI = new PinDeviceAPI();
        this.components = {};
        this.state = {
            connected: false,
            deviceInfo: null,
            wifiNetworks: [],
            plugins: [],
            settings: {}
        };
        
        this.init();
    }
    
    async init() {
        console.log('Initializing Pin Config App...');
        
        // 注册Service Worker
        await this.registerServiceWorker();
        
        // 初始化组件
        this.initComponents();
        
        // 绑定事件监听器
        this.bindEventListeners();
        
        // 加载初始数据
        await this.loadInitialData();
        
        // 设置定期状态更新
        this.startStatusPolling();
        
        console.log('Pin Config App initialized successfully');
    }
    
    async registerServiceWorker() {
        if ('serviceWorker' in navigator) {
            try {
                const registration = await navigator.serviceWorker.register('/sw.js');
                console.log('Service Worker registered:', registration.scope);
                
                // 监听Service Worker更新
                registration.addEventListener('updatefound', () => {
                    const newWorker = registration.installing;
                    newWorker.addEventListener('statechange', () => {
                        if (newWorker.state === 'installed' && navigator.serviceWorker.controller) {
                            this.showToast('应用已更新，刷新页面以使用新版本', 'warning');
                        }
                    });
                });
            } catch (error) {
                console.error('Service Worker registration failed:', error);
            }
        }
    }
    
    initComponents() {
        // 初始化各个组件
        this.components.wifiConfig = new WiFiConfigComponent(this.deviceAPI, this);
        this.components.pluginManager = new PluginManagerComponent(this.deviceAPI, this);
        this.components.displaySettings = new DisplaySettingsComponent(this.deviceAPI, this);
        this.components.deviceStatus = new DeviceStatusComponent(this.deviceAPI, this);
        this.components.systemSettings = new SystemSettingsComponent(this.deviceAPI, this);
    }
    
    bindEventListeners() {
        // 全局错误处理
        window.addEventListener('error', (event) => {
            console.error('Global error:', event.error);
            this.showToast('发生未知错误，请刷新页面重试', 'error');
        });
        
        // 网络状态变化
        window.addEventListener('online', () => {
            this.showToast('网络连接已恢复', 'success');
            this.loadInitialData();
        });
        
        window.addEventListener('offline', () => {
            this.showToast('网络连接已断开，部分功能可能不可用', 'warning');
        });
        
        // 页面可见性变化
        document.addEventListener('visibilitychange', () => {
            if (document.visibilityState === 'visible') {
                this.loadInitialData();
            }
        });
    }
    
    async loadInitialData() {
        try {
            this.updateConnectionStatus('connecting');
            
            // 并行加载各种数据
            const promises = [
                this.components.deviceStatus.loadDeviceInfo(),
                this.components.wifiConfig.scanNetworks(),
                this.components.pluginManager.loadPlugins(),
                this.components.displaySettings.loadSettings()
            ];
            
            await Promise.allSettled(promises);
            
            this.updateConnectionStatus('connected');
            this.state.connected = true;
        } catch (error) {
            console.error('Failed to load initial data:', error);
            this.updateConnectionStatus('disconnected');
            this.state.connected = false;
            this.showToast('连接设备失败，请检查网络连接', 'error');
        }
    }
    
    updateConnectionStatus(status) {
        const statusElement = document.getElementById('connection-status');
        const indicator = statusElement.querySelector('.status-indicator');
        const text = statusElement.querySelector('span:last-child');
        
        // 移除所有状态类
        indicator.classList.remove('status-connected', 'status-disconnected', 'status-connecting');
        
        switch (status) {
            case 'connected':
                indicator.classList.add('status-connected');
                text.textContent = '已连接';
                break;
            case 'disconnected':
                indicator.classList.add('status-disconnected');
                text.textContent = '连接失败';
                break;
            case 'connecting':
                indicator.classList.add('status-connecting');
                text.textContent = '连接中...';
                break;
        }
    }
    
    startStatusPolling() {
        // 每10秒检查一次设备状态
        setInterval(async () => {
            if (this.state.connected) {
                try {
                    await this.components.deviceStatus.updateStatus();
                } catch (error) {
                    console.error('Status polling failed:', error);
                    this.updateConnectionStatus('disconnected');
                    this.state.connected = false;
                }
            }
        }, 10000);
    }
    
    showToast(message, type = 'info', duration = 3000) {
        const container = document.getElementById('toast-container');
        const toast = document.createElement('div');
        
        toast.className = `toast toast-${type}`;
        toast.textContent = message;
        
        container.appendChild(toast);
        
        // 触发显示动画
        setTimeout(() => toast.classList.add('show'), 100);
        
        // 自动隐藏
        setTimeout(() => {
            toast.classList.remove('show');
            setTimeout(() => container.removeChild(toast), 300);
        }, duration);
    }
    
    // 组件间通信方法
    emit(event, data) {
        console.log('Event emitted:', event, data);
        
        // 处理跨组件事件
        switch (event) {
            case 'wifi-connected':
                this.showToast('WiFi连接成功', 'success');
                this.components.deviceStatus.loadDeviceInfo();
                break;
            case 'plugin-enabled':
                this.showToast(`插件 "${data.name}" 已启用`, 'success');
                break;
            case 'settings-applied':
                this.showToast('设置已应用', 'success');
                break;
        }
    }
}

// 设备API通信类
class PinDeviceAPI {
    constructor() {
        this.baseURL = window.location.origin;
        this.timeout = 10000;
    }
    
    async request(endpoint, options = {}) {
        const url = `${this.baseURL}${endpoint}`;
        const config = {
            headers: {
                'Content-Type': 'application/json',
                ...options.headers
            },
            timeout: this.timeout,
            ...options
        };
        
        // 添加请求超时处理
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), this.timeout);
        config.signal = controller.signal;
        
        try {
            const response = await fetch(url, config);
            clearTimeout(timeoutId);
            
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }
            
            const contentType = response.headers.get('content-type');
            if (contentType && contentType.includes('application/json')) {
                return await response.json();
            }
            
            return await response.text();
        } catch (error) {
            clearTimeout(timeoutId);
            if (error.name === 'AbortError') {
                throw new Error('请求超时，请检查设备连接');
            }
            throw error;
        }
    }
    
    // API方法
    async getDeviceStatus() {
        return this.request('/api/status');
    }
    
    async scanWiFiNetworks() {
        return this.request('/api/wifi/scan');
    }
    
    async connectWiFi(ssid, password) {
        return this.request('/api/wifi/connect', {
            method: 'POST',
            body: JSON.stringify({ ssid, password })
        });
    }
    
    async getPlugins() {
        return this.request('/api/plugins');
    }
    
    async togglePlugin(pluginName, enabled) {
        return this.request(`/api/plugins/${pluginName}`, {
            method: 'POST',
            body: JSON.stringify({ enabled })
        });
    }
    
    async getSettings() {
        return this.request('/api/settings');
    }
    
    async updateSettings(settings) {
        return this.request('/api/settings', {
            method: 'POST',
            body: JSON.stringify(settings)
        });
    }
    
    async restartDevice() {
        return this.request('/api/system/restart', { method: 'POST' });
    }
    
    async factoryReset() {
        return this.request('/api/system/factory-reset', { method: 'POST' });
    }
    
    async checkUpdate() {
        return this.request('/api/system/check-update');
    }
}

// 初始化应用
document.addEventListener('DOMContentLoaded', () => {
    window.pinApp = new PinConfigApp();
});

// 导出给其他模块使用
window.PinDeviceAPI = PinDeviceAPI;
```

### Service Worker

```javascript
// sw.js - Service Worker
const CACHE_NAME = 'pin-config-v1.0.0';
const STATIC_ASSETS = [
    '/',
    '/index.html',
    '/app.js',
    '/manifest.json',
    '/assets/icons/icon-192.png',
    '/assets/icons/icon-512.png'
];

// 安装事件 - 缓存静态资源
self.addEventListener('install', (event) => {
    console.log('Service Worker installing...');
    
    event.waitUntil(
        caches.open(CACHE_NAME)
            .then((cache) => {
                console.log('Caching static assets...');
                return cache.addAll(STATIC_ASSETS);
            })
            .then(() => {
                console.log('Static assets cached successfully');
                return self.skipWaiting();
            })
            .catch((error) => {
                console.error('Failed to cache static assets:', error);
            })
    );
});

// 激活事件 - 清理旧缓存
self.addEventListener('activate', (event) => {
    console.log('Service Worker activating...');
    
    event.waitUntil(
        caches.keys()
            .then((cacheNames) => {
                return Promise.all(
                    cacheNames.map((cacheName) => {
                        if (cacheName !== CACHE_NAME) {
                            console.log('Deleting old cache:', cacheName);
                            return caches.delete(cacheName);
                        }
                    })
                );
            })
            .then(() => {
                console.log('Service Worker activated');
                return self.clients.claim();
            })
    );
});

// 拦截请求 - 缓存策略
self.addEventListener('fetch', (event) => {
    const request = event.request;
    
    // 只处理GET请求
    if (request.method !== 'GET') {
        return;
    }
    
    // API请求 - 网络优先策略
    if (request.url.includes('/api/')) {
        event.respondWith(
            fetch(request)
                .then((response) => {
                    // 缓存成功的API响应
                    if (response.ok) {
                        const responseClone = response.clone();
                        caches.open(CACHE_NAME)
                            .then((cache) => {
                                cache.put(request, responseClone);
                            });
                    }
                    return response;
                })
                .catch(() => {
                    // 网络失败时尝试从缓存获取
                    return caches.match(request)
                        .then((cachedResponse) => {
                            if (cachedResponse) {
                                return cachedResponse;
                            }
                            // 返回离线页面或默认响应
                            return new Response(
                                JSON.stringify({ error: '网络连接失败，请检查设备连接' }),
                                { 
                                    status: 503,
                                    headers: { 'Content-Type': 'application/json' }
                                }
                            );
                        });
                })
        );
        return;
    }
    
    // 静态资源 - 缓存优先策略
    event.respondWith(
        caches.match(request)
            .then((cachedResponse) => {
                if (cachedResponse) {
                    return cachedResponse;
                }
                
                return fetch(request)
                    .then((response) => {
                        // 只缓存成功的响应
                        if (response.ok) {
                            const responseClone = response.clone();
                            caches.open(CACHE_NAME)
                                .then((cache) => {
                                    cache.put(request, responseClone);
                                });
                        }
                        return response;
                    })
                    .catch(() => {
                        // 返回离线提示页面
                        if (request.url.endsWith('.html') || request.headers.get('accept').includes('text/html')) {
                            return new Response(`
                                <!DOCTYPE html>
                                <html>
                                <head>
                                    <title>离线模式</title>
                                    <meta name="viewport" content="width=device-width, initial-scale=1.0">
                                    <style>
                                        body { font-family: sans-serif; text-align: center; padding: 50px; }
                                        .offline { color: #666; }
                                    </style>
                                </head>
                                <body>
                                    <div class="offline">
                                        <h1>离线模式</h1>
                                        <p>无法连接到Pin设备，请检查网络连接。</p>
                                        <button onclick="location.reload()">重试</button>
                                    </div>
                                </body>
                                </html>
                            `, {
                                headers: { 'Content-Type': 'text/html' }
                            });
                        }
                        
                        return new Response('资源不可用', { status: 404 });
                    });
            })
    );
});

// 后台同步 (如果支持)
self.addEventListener('sync', (event) => {
    if (event.tag === 'background-sync') {
        console.log('Background sync triggered');
        
        event.waitUntil(
            // 执行后台同步任务
            syncData()
        );
    }
});

async function syncData() {
    try {
        // 同步离线期间的设置变更
        const cache = await caches.open(CACHE_NAME);
        const requests = await cache.keys();
        
        for (const request of requests) {
            if (request.url.includes('/api/') && request.method === 'POST') {
                // 重新发送POST请求
                await fetch(request);
            }
        }
        
        console.log('Background sync completed');
    } catch (error) {
        console.error('Background sync failed:', error);
    }
}

// 推送通知处理 (预留)
self.addEventListener('push', (event) => {
    if (event.data) {
        const data = event.data.json();
        
        event.waitUntil(
            self.registration.showNotification(data.title, {
                body: data.body,
                icon: '/assets/icons/icon-192.png',
                badge: '/assets/icons/badge-72.png',
                tag: 'pin-notification',
                requireInteraction: false
            })
        );
    }
});

// 通知点击处理
self.addEventListener('notificationclick', (event) => {
    event.notification.close();
    
    event.waitUntil(
        self.clients.matchAll().then((clients) => {
            // 如果已有打开的页面，聚焦到它
            for (const client of clients) {
                if (client.url.includes(self.registration.scope)) {
                    return client.focus();
                }
            }
            
            // 否则打开新页面
            return self.clients.openWindow('/');
        })
    );
});
```

## 组件模块

### WiFi配置组件

```javascript
// components/wifi-config.js
class WiFiConfigComponent {
    constructor(api, app) {
        this.api = api;
        this.app = app;
        this.isScanning = false;
        this.isConnecting = false;
        
        this.initElements();
        this.bindEvents();
    }
    
    initElements() {
        this.ssidSelect = document.getElementById('wifi-ssid');
        this.passwordInput = document.getElementById('wifi-password');
        this.connectBtn = document.getElementById('wifi-connect-btn');
        this.scanBtn = document.getElementById('wifi-scan-btn');
        this.form = document.getElementById('wifi-form');
    }
    
    bindEvents() {
        this.form.addEventListener('submit', this.handleConnect.bind(this));
        this.scanBtn.addEventListener('click', this.handleScan.bind(this));
        this.ssidSelect.addEventListener('change', this.handleSSIDChange.bind(this));
    }
    
    async scanNetworks() {
        if (this.isScanning) return;
        
        this.isScanning = true;
        this.updateScanButton(true);
        
        try {
            const response = await this.api.scanWiFiNetworks();
            this.renderNetworks(response.networks || []);
        } catch (error) {
            console.error('WiFi scan failed:', error);
            this.app.showToast('扫描WiFi网络失败', 'error');
        } finally {
            this.isScanning = false;
            this.updateScanButton(false);
        }
    }
    
    renderNetworks(networks) {
        // 清空现有选项
        this.ssidSelect.innerHTML = '<option value="">选择WiFi网络...</option>';
        
        // 按信号强度排序
        networks.sort((a, b) => b.rssi - a.rssi);
        
        networks.forEach((network) => {
            const option = document.createElement('option');
            option.value = network.ssid;
            option.textContent = `${network.ssid} (${this.getSignalStrength(network.rssi)})`;
            
            // 添加安全标识
            if (network.auth !== 0) {
                option.textContent += ' 🔒';
            }
            
            this.ssidSelect.appendChild(option);
        });
        
        if (networks.length === 0) {
            const option = document.createElement('option');
            option.textContent = '未发现WiFi网络';
            option.disabled = true;
            this.ssidSelect.appendChild(option);
        }
    }
    
    getSignalStrength(rssi) {
        if (rssi >= -30) return '优秀';
        if (rssi >= -50) return '良好';
        if (rssi >= -60) return '一般';
        if (rssi >= -70) return '较弱';
        return '很弱';
    }
    
    async handleConnect(event) {
        event.preventDefault();
        
        const ssid = this.ssidSelect.value;
        const password = this.passwordInput.value;
        
        if (!ssid) {
            this.app.showToast('请选择WiFi网络', 'warning');
            return;
        }
        
        if (this.isConnecting) return;
        
        this.isConnecting = true;
        this.updateConnectButton(true);
        
        try {
            const response = await this.api.connectWiFi(ssid, password);
            
            if (response.success) {
                this.app.showToast(`已连接到 ${ssid}`, 'success');
                this.app.emit('wifi-connected', { ssid });
                this.passwordInput.value = '';
            } else {
                throw new Error(response.message || '连接失败');
            }
        } catch (error) {
            console.error('WiFi connection failed:', error);
            this.app.showToast(`连接失败: ${error.message}`, 'error');
        } finally {
            this.isConnecting = false;
            this.updateConnectButton(false);
        }
    }
    
    handleScan() {
        this.scanNetworks();
    }
    
    handleSSIDChange() {
        const ssid = this.ssidSelect.value;
        this.passwordInput.disabled = !ssid;
        
        if (!ssid) {
            this.passwordInput.value = '';
        }
    }
    
    updateScanButton(scanning) {
        if (scanning) {
            this.scanBtn.innerHTML = '<span class="loading"></span> 扫描中...';
            this.scanBtn.disabled = true;
        } else {
            this.scanBtn.innerHTML = '扫描网络';
            this.scanBtn.disabled = false;
        }
    }
    
    updateConnectButton(connecting) {
        if (connecting) {
            this.connectBtn.innerHTML = '<span class="loading"></span> 连接中...';
            this.connectBtn.disabled = true;
        } else {
            this.connectBtn.innerHTML = '连接 WiFi';
            this.connectBtn.disabled = false;
        }
    }
}

window.WiFiConfigComponent = WiFiConfigComponent;
```

## ESP32-C3 Web服务器集成

### HTTP服务器配置

```c
// components/pin_webserver/pin_webserver.c
#include "pin_webserver.h"
#include "esp_http_server.h"
#include "esp_vfs.h"
#include "cJSON.h"

static httpd_handle_t g_server = NULL;
static const char* TAG = "PIN_WEBSERVER";

// 文件MIME类型映射
static const struct {
    const char* ext;
    const char* type;
} mime_types[] = {
    {".html", "text/html"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".json", "application/json"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".ico", "image/x-icon"},
    {".svg", "image/svg+xml"}
};

// 获取文件MIME类型
static const char* get_mime_type(const char* filename) {
    const char* dot = strrchr(filename, '.');
    if (dot) {
        for (int i = 0; i < sizeof(mime_types) / sizeof(mime_types[0]); i++) {
            if (strcasecmp(dot, mime_types[i].ext) == 0) {
                return mime_types[i].type;
            }
        }
    }
    return "text/plain";
}

// 根路径处理器
static esp_err_t root_handler(httpd_req_t *req) {
    extern const unsigned char index_html_start[] asm("_binary_index_html_start");
    extern const unsigned char index_html_end[] asm("_binary_index_html_end");
    const size_t index_html_size = (index_html_end - index_html_start);
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
    return httpd_resp_send(req, (const char*)index_html_start, index_html_size);
}

// 静态文件处理器
static esp_err_t static_handler(httpd_req_t *req) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "/spiffs%s", req->uri);
    
    // 安全检查 - 防止目录遍历
    if (strstr(req->uri, "..") != NULL) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Access denied");
        return ESP_FAIL;
    }
    
    FILE* file = fopen(filepath, "r");
    if (!file) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }
    
    // 设置内容类型
    const char* content_type = get_mime_type(req->uri);
    httpd_resp_set_type(req, content_type);
    
    // 设置缓存头
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
    
    // 发送文件内容
    char buffer[1024];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (httpd_resp_send_chunk(req, buffer, bytes_read) != ESP_OK) {
            fclose(file);
            return ESP_FAIL;
        }
    }
    
    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// API: 获取设备状态
static esp_err_t api_status_handler(httpd_req_t *req) {
    cJSON *json = cJSON_CreateObject();
    
    // 设备基本信息
    cJSON *device = cJSON_CreateObject();
    cJSON_AddStringToObject(device, "name", "Pin Device");
    cJSON_AddStringToObject(device, "version", "1.0.0");
    cJSON_AddStringToObject(device, "chip", "ESP32-C3");
    
    // 电池信息
    cJSON *battery = cJSON_CreateObject();
    float voltage = pin_battery_get_voltage();
    uint8_t percentage = pin_battery_get_percentage(voltage);
    cJSON_AddNumberToObject(battery, "voltage", voltage);
    cJSON_AddNumberToObject(battery, "percentage", percentage);
    
    // WiFi信息
    cJSON *wifi = cJSON_CreateObject();
    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret == ESP_OK) {
        cJSON_AddStringToObject(wifi, "ssid", (char*)ap_info.ssid);
        cJSON_AddNumberToObject(wifi, "rssi", ap_info.rssi);
        cJSON_AddBoolToObject(wifi, "connected", true);
    } else {
        cJSON_AddBoolToObject(wifi, "connected", false);
    }
    
    cJSON_AddItemToObject(json, "device", device);
    cJSON_AddItemToObject(json, "battery", battery);
    cJSON_AddItemToObject(json, "wifi", wifi);
    
    const char *json_string = cJSON_Print(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string, strlen(json_string));
    
    free((void*)json_string);
    cJSON_Delete(json);
    return ESP_OK;
}

// 启动Web服务器
esp_err_t pin_webserver_start(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 20;
    config.max_resp_headers = 8;
    config.stack_size = 8192;
    
    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);
    
    if (httpd_start(&g_server, &config) == ESP_OK) {
        // 注册路由处理器
        
        // 根路径
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &root_uri);
        
        // API路由
        httpd_uri_t api_status_uri = {
            .uri = "/api/status",
            .method = HTTP_GET,
            .handler = api_status_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &api_status_uri);
        
        // 静态文件路由 (需要在最后注册)
        httpd_uri_t static_uri = {
            .uri = "/*",
            .method = HTTP_GET,
            .handler = static_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_server, &static_uri);
        
        ESP_LOGI(TAG, "Web server started successfully");
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Failed to start web server");
    return ESP_FAIL;
}
```

## 性能优化

### 资源压缩和缓存

1. **文件压缩**: CSS/JS内联，减少HTTP请求
2. **图片优化**: 使用SVG图标，WebP格式图片
3. **缓存策略**: 静态资源长期缓存，API响应短期缓存
4. **懒加载**: 非关键组件按需加载

### 内存优化

1. **组件复用**: 避免重复创建DOM元素
2. **事件委托**: 减少事件监听器数量
3. **数据分页**: 大列表分页加载
4. **定时清理**: 定期清理无用缓存

## 测试和调试

### 自动化测试

```javascript
// 简单的测试框架
class PinTestRunner {
    constructor() {
        this.tests = [];
        this.results = { passed: 0, failed: 0 };
    }
    
    test(name, fn) {
        this.tests.push({ name, fn });
    }
    
    async run() {
        console.log('Running PWA tests...');
        
        for (const test of this.tests) {
            try {
                await test.fn();
                console.log(`✓ ${test.name}`);
                this.results.passed++;
            } catch (error) {
                console.error(`✗ ${test.name}: ${error.message}`);
                this.results.failed++;
            }
        }
        
        console.log(`Tests completed: ${this.results.passed} passed, ${this.results.failed} failed`);
    }
}

// 测试用例
const testRunner = new PinTestRunner();

testRunner.test('Service Worker registration', async () => {
    if (!('serviceWorker' in navigator)) {
        throw new Error('Service Worker not supported');
    }
});

testRunner.test('PWA manifest', async () => {
    const response = await fetch('/manifest.json');
    if (!response.ok) {
        throw new Error('Manifest not found');
    }
});

testRunner.test('API connectivity', async () => {
    const api = new PinDeviceAPI();
    await api.getDeviceStatus();
});
```

Pin PWA应用为用户提供了现代化的设备配置体验，通过渐进式Web应用技术实现了原生应用般的使用体验，同时保持了Web技术的便利性和跨平台特性。