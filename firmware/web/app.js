/**
 * Pin Device Configuration PWA
 * Main Application Logic
 */

class PinConfigApp {
    constructor() {
        this.deviceAPI = new PinDeviceAPI();
        this.state = {
            connected: false,
            deviceInfo: null,
            wifiNetworks: [],
            selectedNetwork: null,
            plugins: [],
            settings: {}
        };
        
        this.init();
    }
    
    async init() {
        console.log('Initializing Pin Config App...');
        
        // 注册Service Worker
        await this.registerServiceWorker();
        
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
            } catch (error) {
                console.error('Service Worker registration failed:', error);
            }
        }
    }
    
    bindEventListeners() {
        // WiFi扫描按钮
        document.getElementById('wifi-scan-btn').addEventListener('click', () => {
            this.scanWiFiNetworks();
        });
        
        // WiFi连接表单
        document.getElementById('wifi-form').addEventListener('submit', (e) => {
            e.preventDefault();
            this.connectToWiFi();
        });
        
        // 显示设置应用按钮
        document.getElementById('apply-display-settings').addEventListener('click', () => {
            this.applyDisplaySettings();
        });
        
        // 系统操作按钮
        document.getElementById('restart-device').addEventListener('click', () => {
            this.restartDevice();
        });
        
        document.getElementById('factory-reset').addEventListener('click', () => {
            this.factoryReset();
        });
        
        document.getElementById('check-update').addEventListener('click', () => {
            this.checkUpdate();
        });
        
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
    }
    
    async loadInitialData() {
        try {
            this.updateConnectionStatus('connecting');
            
            // 并行加载各种数据
            const promises = [
                this.loadDeviceInfo(),
                this.loadPlugins(),
                this.loadSettings()
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
    
    async loadDeviceInfo() {
        try {
            const deviceInfo = await this.deviceAPI.getDeviceStatus();
            this.state.deviceInfo = deviceInfo;
            this.renderDeviceInfo(deviceInfo);
        } catch (error) {
            console.error('Failed to load device info:', error);
            throw error;
        }
    }
    
    async loadPlugins() {
        try {
            const plugins = await this.deviceAPI.getPlugins();
            this.state.plugins = plugins;
            this.renderPlugins(plugins);
        } catch (error) {
            console.error('Failed to load plugins:', error);
            this.renderPluginsError();
        }
    }
    
    async loadSettings() {
        try {
            const settings = await this.deviceAPI.getSettings();
            this.state.settings = settings;
            this.renderSettings(settings);
        } catch (error) {
            console.error('Failed to load settings:', error);
        }
    }
    
    async scanWiFiNetworks() {
        const scanBtn = document.getElementById('wifi-scan-btn');
        const btnText = scanBtn.querySelector('span');
        
        // 更新按钮状态
        const originalText = btnText.textContent;
        btnText.innerHTML = '<span class="loading"></span>扫描中...';
        scanBtn.disabled = true;
        
        try {
            const response = await this.deviceAPI.scanWiFiNetworks();
            this.state.wifiNetworks = response.networks || [];
            this.renderWiFiNetworks(this.state.wifiNetworks);
            
            if (this.state.wifiNetworks.length === 0) {
                this.showToast('未发现WiFi网络', 'warning');
            }
        } catch (error) {
            console.error('WiFi scan failed:', error);
            this.showToast('扫描WiFi网络失败', 'error');
        } finally {
            // 恢复按钮状态
            btnText.textContent = originalText;
            scanBtn.disabled = false;
        }
    }
    
    async connectToWiFi() {
        if (!this.state.selectedNetwork) {
            this.showToast('请先选择WiFi网络', 'warning');
            return;
        }
        
        const password = document.getElementById('wifi-password').value;
        const connectBtn = document.getElementById('wifi-connect-btn');
        const btnText = connectBtn.querySelector('span');
        
        // 更新按钮状态
        const originalText = btnText.textContent;
        btnText.innerHTML = '<span class="loading"></span>连接中...';
        connectBtn.disabled = true;
        
        try {
            const response = await this.deviceAPI.connectWiFi(this.state.selectedNetwork.ssid, password);
            
            if (response.success) {
                this.showToast(`已连接到 ${this.state.selectedNetwork.ssid}`, 'success');
                // 等待一段时间后重新加载设备信息
                setTimeout(() => {
                    this.loadDeviceInfo();
                }, 3000);
            } else {
                throw new Error(response.message || '连接失败');
            }
        } catch (error) {
            console.error('WiFi connection failed:', error);
            this.showToast(`连接失败: ${error.message}`, 'error');
        } finally {
            // 恢复按钮状态
            btnText.textContent = originalText;
            connectBtn.disabled = false;
        }
    }
    
    async applyDisplaySettings() {
        const settings = {
            refresh_interval: parseInt(document.getElementById('refresh-interval').value),
            power_save_mode: document.getElementById('power-save-mode').checked,
            auto_brightness: document.getElementById('auto-brightness').checked
        };
        
        const btn = document.getElementById('apply-display-settings');
        btn.disabled = true;
        
        try {
            await this.deviceAPI.updateSettings(settings);
            this.showToast('设置已应用', 'success');
        } catch (error) {
            console.error('Failed to apply settings:', error);
            this.showToast('设置应用失败', 'error');
        } finally {
            btn.disabled = false;
        }
    }
    
    async togglePlugin(pluginName, enabled) {
        try {
            await this.deviceAPI.togglePlugin(pluginName, enabled);
            this.showToast(`插件 "${pluginName}" ${enabled ? '已启用' : '已禁用'}`, 'success');
            
            // 更新插件状态
            const plugin = this.state.plugins.find(p => p.name === pluginName);
            if (plugin) {
                plugin.enabled = enabled;
            }
        } catch (error) {
            console.error('Failed to toggle plugin:', error);
            this.showToast(`插件操作失败: ${error.message}`, 'error');
        }
    }
    
    async restartDevice() {
        if (!confirm('确定要重启设备吗？')) return;
        
        try {
            await this.deviceAPI.restartDevice();
            this.showToast('设备正在重启...', 'success');
        } catch (error) {
            console.error('Failed to restart device:', error);
            this.showToast('重启失败', 'error');
        }
    }
    
    async factoryReset() {
        if (!confirm('确定要恢复出厂设置吗？这将清除所有配置数据。')) return;
        
        try {
            await this.deviceAPI.factoryReset();
            this.showToast('正在恢复出厂设置...', 'success');
        } catch (error) {
            console.error('Failed to factory reset:', error);
            this.showToast('恢复出厂设置失败', 'error');
        }
    }
    
    async checkUpdate() {
        const btn = document.getElementById('check-update');
        btn.disabled = true;
        
        try {
            const response = await this.deviceAPI.checkUpdate();
            if (response.update_available) {
                this.showToast(`发现新版本: ${response.version}`, 'success');
            } else {
                this.showToast('当前已是最新版本', 'success');
            }
        } catch (error) {
            console.error('Failed to check update:', error);
            this.showToast('检查更新失败', 'error');
        } finally {
            btn.disabled = false;
        }
    }
    
    // 渲染方法
    renderDeviceInfo(deviceInfo) {
        const container = document.getElementById('device-info');
        
        container.innerHTML = `
            <div class="device-info">
                <h3>${deviceInfo.device.name}</h3>
                <p>版本: ${deviceInfo.device.version} | 芯片: ${deviceInfo.device.chip}</p>
                <p>WiFi: ${deviceInfo.wifi.connected ? deviceInfo.wifi.ssid : '未连接'}</p>
            </div>
            <div class="battery-info">
                <div class="battery-level">
                    <div class="battery-fill ${this.getBatteryClass(deviceInfo.battery.percentage)}" 
                         style="width: ${deviceInfo.battery.percentage}%"></div>
                </div>
                <span>${deviceInfo.battery.percentage}%</span>
            </div>
        `;
    }
    
    renderWiFiNetworks(networks) {
        const networksDiv = document.getElementById('wifi-networks');
        const networkList = document.getElementById('network-list');
        
        if (networks.length === 0) {
            networksDiv.classList.add('hidden');
            return;
        }
        
        networkList.innerHTML = '';
        
        networks.forEach(network => {
            const networkDiv = document.createElement('div');
            networkDiv.className = 'network-item';
            networkDiv.innerHTML = `
                <div class="network-info">
                    <h4>${network.ssid} ${network.auth !== 0 ? '🔒' : ''}</h4>
                    <p>信号强度: ${this.getSignalStrength(network.rssi)} | 频道: ${network.channel}</p>
                </div>
                <div class="signal-strength">
                    ${this.renderSignalBars(network.rssi)}
                </div>
            `;
            
            networkDiv.addEventListener('click', () => {
                this.selectNetwork(network, networkDiv);
            });
            
            networkList.appendChild(networkDiv);
        });
        
        networksDiv.classList.remove('hidden');
    }
    
    selectNetwork(network, element) {
        // 移除其他选中状态
        document.querySelectorAll('.network-item').forEach(item => {
            item.classList.remove('selected');
        });
        
        // 选中当前网络
        element.classList.add('selected');
        this.state.selectedNetwork = network;
        
        // 显示WiFi表单
        const form = document.getElementById('wifi-form');
        form.classList.remove('hidden');
        
        // 设置SSID
        document.getElementById('wifi-ssid').value = network.ssid;
        
        // 清空密码
        document.getElementById('wifi-password').value = '';
        document.getElementById('wifi-password').focus();
    }
    
    renderPlugins(plugins) {
        const container = document.getElementById('plugin-list');
        
        if (!plugins || plugins.length === 0) {
            container.innerHTML = '<p>暂无可用插件</p>';
            return;
        }
        
        container.innerHTML = '';
        
        plugins.forEach(plugin => {
            const pluginDiv = document.createElement('div');
            pluginDiv.className = 'plugin-item';
            pluginDiv.innerHTML = `
                <div class="plugin-info">
                    <h4>${plugin.name}</h4>
                    <p>${plugin.description}</p>
                </div>
                <label class="switch">
                    <input type="checkbox" ${plugin.enabled ? 'checked' : ''} 
                           onchange="window.pinApp.togglePlugin('${plugin.name}', this.checked)">
                    <span class="slider"></span>
                </label>
            `;
            
            container.appendChild(pluginDiv);
        });
    }
    
    renderPluginsError() {
        const container = document.getElementById('plugin-list');
        container.innerHTML = '<p style="color: var(--error-color);">加载插件失败</p>';
    }
    
    renderSettings(settings) {
        if (settings.refresh_interval) {
            document.getElementById('refresh-interval').value = settings.refresh_interval;
        }
        if (typeof settings.power_save_mode === 'boolean') {
            document.getElementById('power-save-mode').checked = settings.power_save_mode;
        }
        if (typeof settings.auto_brightness === 'boolean') {
            document.getElementById('auto-brightness').checked = settings.auto_brightness;
        }
    }
    
    // 工具方法
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
        // 每30秒检查一次设备状态
        setInterval(async () => {
            if (this.state.connected) {
                try {
                    await this.loadDeviceInfo();
                } catch (error) {
                    console.error('Status polling failed:', error);
                    this.updateConnectionStatus('disconnected');
                    this.state.connected = false;
                }
            }
        }, 30000);
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
            setTimeout(() => {
                if (container.contains(toast)) {
                    container.removeChild(toast);
                }
            }, 300);
        }, duration);
    }
    
    getBatteryClass(percentage) {
        if (percentage > 60) return 'battery-high';
        if (percentage > 20) return 'battery-medium';
        return 'battery-low';
    }
    
    getSignalStrength(rssi) {
        if (rssi >= -30) return '优秀';
        if (rssi >= -50) return '良好';
        if (rssi >= -60) return '一般';
        if (rssi >= -70) return '较弱';
        return '很弱';
    }
    
    renderSignalBars(rssi) {
        const bars = [];
        const strength = Math.min(4, Math.max(0, Math.floor((rssi + 100) / 15)));
        
        for (let i = 0; i < 4; i++) {
            const height = (i + 1) * 4 + 4;
            const active = i < strength ? 'active' : '';
            bars.push(`<div class="signal-bar ${active}" style="height: ${height}px;"></div>`);
        }
        
        return bars.join('');
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