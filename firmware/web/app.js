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
            settings: {},
            canvases: [],
            currentCanvas: null,
            selectedElement: null,
            canvasEditor: {
                mode: 'select', // select, text, image, shape
                zoom: 1.0,
                showGrid: true
            }
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
        
        // 加载画布数据
        await this.loadCanvases();
        
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
    
    async loadCanvases() {
        try {
            const response = await this.deviceAPI.getCanvases();
            this.state.canvases = response.canvases || [];
            this.renderCanvasesList(this.state.canvases);
        } catch (error) {
            console.error('Failed to load canvases:', error);
        }
    }
    
    async createNewCanvas() {
        const name = prompt('输入画布名称:');
        if (!name) return;
        
        const id = 'canvas_' + Date.now();
        
        try {
            await this.deviceAPI.createCanvas(id, name);
            await this.loadCanvases();
            this.showToast('画布创建成功', 'success');
        } catch (error) {
            console.error('Failed to create canvas:', error);
            this.showToast('画布创建失败', 'error');
        }
    }
    
    async selectCanvas(canvasId) {
        try {
            const canvas = await this.deviceAPI.getCanvas(canvasId);
            this.state.currentCanvas = canvas;
            this.renderCanvasEditor(canvas);
            this.showCanvasEditor();
        } catch (error) {
            console.error('Failed to load canvas:', error);
            this.showToast('加载画布失败', 'error');
        }
    }
    
    async deleteCanvas(canvasId) {
        if (!confirm('确定要删除这个画布吗？')) return;
        
        try {
            await this.deviceAPI.deleteCanvas(canvasId);
            await this.loadCanvases();
            if (this.state.currentCanvas && this.state.currentCanvas.id === canvasId) {
                this.state.currentCanvas = null;
                this.hideCanvasEditor();
            }
            this.showToast('画布删除成功', 'success');
        } catch (error) {
            console.error('Failed to delete canvas:', error);
            this.showToast('画布删除失败', 'error');
        }
    }
    
    async displayCanvas(canvasId) {
        try {
            await this.deviceAPI.displayCanvas(canvasId);
            this.showToast('画布已显示到设备', 'success');
        } catch (error) {
            console.error('Failed to display canvas:', error);
            this.showToast('画布显示失败', 'error');
        }
    }
    
    async saveCurrentCanvas() {
        if (!this.state.currentCanvas) return;
        
        try {
            await this.deviceAPI.updateCanvas(this.state.currentCanvas);
            this.showToast('画布保存成功', 'success');
        } catch (error) {
            console.error('Failed to save canvas:', error);
            this.showToast('画布保存失败', 'error');
        }
    }
    
    addTextElement() {
        if (!this.state.currentCanvas) return;
        
        const text = prompt('输入文字内容:');
        if (!text) return;
        
        const element = {
            id: 'text_' + Date.now(),
            type: 0, // PIN_CANVAS_ELEMENT_TEXT
            x: 50,
            y: 50,
            width: 200,
            height: 50,
            z_index: 0,
            visible: true,
            props: {
                text: text,
                font_size: 16,
                color: 0, // BLACK
                align: 0, // LEFT
                bold: false,
                italic: false
            }
        };
        
        this.state.currentCanvas.elements = this.state.currentCanvas.elements || [];
        this.state.currentCanvas.elements.push(element);
        this.renderCanvasEditor(this.state.currentCanvas);
    }
    
    addImageElement() {
        if (!this.state.currentCanvas) return;
        
        const fileInput = document.createElement('input');
        fileInput.type = 'file';
        fileInput.accept = 'image/*';
        fileInput.onchange = async (e) => {
            const file = e.target.files[0];
            if (!file) return;
            
            const imageId = 'img_' + Date.now();
            
            try {
                await this.deviceAPI.uploadImage(imageId, file);
                
                const element = {
                    id: 'image_' + Date.now(),
                    type: 1, // PIN_CANVAS_ELEMENT_IMAGE
                    x: 50,
                    y: 50,
                    width: 100,
                    height: 100,
                    z_index: 0,
                    visible: true,
                    props: {
                        image_id: imageId,
                        format: 0,
                        maintain_aspect_ratio: true,
                        opacity: 255
                    }
                };
                
                this.state.currentCanvas.elements = this.state.currentCanvas.elements || [];
                this.state.currentCanvas.elements.push(element);
                this.renderCanvasEditor(this.state.currentCanvas);
                this.showToast('图片添加成功', 'success');
            } catch (error) {
                console.error('Failed to upload image:', error);
                this.showToast('图片上传失败', 'error');
            }
        };
        fileInput.click();
    }
    
    // Template and preset methods
    createTemplate(name, description) {
        if (!this.state.currentCanvas) return;
        
        const template = {
            id: 'template_' + Date.now(),
            name: name,
            description: description,
            canvas: JSON.parse(JSON.stringify(this.state.currentCanvas)) // Deep copy
        };
        
        // Store in localStorage for now
        const templates = JSON.parse(localStorage.getItem('pin_canvas_templates') || '[]');
        templates.push(template);
        localStorage.setItem('pin_canvas_templates', JSON.stringify(templates));
        
        this.showToast('模板创建成功', 'success');
    }
    
    loadTemplate(templateId) {
        const templates = JSON.parse(localStorage.getItem('pin_canvas_templates') || '[]');
        const template = templates.find(t => t.id === templateId);
        
        if (!template) {
            this.showToast('模板不存在', 'error');
            return;
        }
        
        // Create new canvas from template
        const newCanvasId = 'canvas_' + Date.now();
        const newCanvas = {
            ...template.canvas,
            id: newCanvasId,
            name: template.name + '_副本',
            created_time: Math.floor(Date.now() / 1000),
            modified_time: Math.floor(Date.now() / 1000)
        };
        
        this.deviceAPI.createCanvas(newCanvasId, newCanvas.name).then(() => {
            return this.deviceAPI.updateCanvas(newCanvas);
        }).then(() => {
            this.loadCanvases();
            this.showToast('从模板创建画布成功', 'success');
        }).catch(error => {
            console.error('Failed to create canvas from template:', error);
            this.showToast('从模板创建画布失败', 'error');
        });
    }
    
    applyPreset(presetId) {
        const presets = [
            {
                id: 'clock_preset',
                name: '时钟显示',
                elements: [
                    {
                        id: 'time_text',
                        type: 0,
                        x: 200, y: 180, width: 200, height: 80,
                        z_index: 0, visible: true,
                        props: { text: '12:34', font_size: 32, color: 0, align: 1, bold: true, italic: false }
                    }
                ]
            },
            {
                id: 'weather_preset',
                name: '天气显示',
                elements: [
                    {
                        id: 'temperature',
                        type: 0,
                        x: 200, y: 150, width: 200, height: 80,
                        z_index: 0, visible: true,
                        props: { text: '23°C', font_size: 48, color: 2, align: 1, bold: true, italic: false }
                    }
                ]
            }
        ];
        
        const preset = presets.find(p => p.id === presetId);
        if (!preset) {
            this.showToast('预设不存在', 'error');
            return;
        }
        
        // Create new canvas with preset
        const canvasId = 'canvas_' + Date.now();
        
        this.deviceAPI.createCanvas(canvasId, preset.name).then(() => {
            const canvas = {
                id: canvasId,
                name: preset.name,
                background_color: 1,
                created_time: Math.floor(Date.now() / 1000),
                modified_time: Math.floor(Date.now() / 1000),
                elements: preset.elements,
                element_count: preset.elements.length
            };
            
            return this.deviceAPI.updateCanvas(canvas);
        }).then(() => {
            this.loadCanvases();
            this.showToast('预设应用成功', 'success');
        }).catch(error => {
            console.error('Failed to apply preset:', error);
            this.showToast('预设应用失败', 'error');
        });
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
    
    renderCanvasesList(canvases) {
        const container = document.getElementById('canvas-list');
        if (!container) return;
        
        if (!canvases || canvases.length === 0) {
            container.innerHTML = '<p>暂无画布，请创建一个新画布</p>';
            return;
        }
        
        container.innerHTML = '';
        canvases.forEach(canvas => {
            const canvasDiv = document.createElement('div');
            canvasDiv.className = 'canvas-item';
            canvasDiv.innerHTML = `
                <div class="canvas-info">
                    <h4>${canvas.name}</h4>
                    <p>元素数量: ${canvas.element_count || 0}</p>
                    <p>修改时间: ${new Date(canvas.modified_time * 1000).toLocaleString()}</p>
                </div>
                <div class="canvas-actions">
                    <button onclick="pinApp.selectCanvas('${canvas.id}')" class="btn btn-primary">编辑</button>
                    <button onclick="pinApp.displayCanvas('${canvas.id}')" class="btn btn-secondary">显示</button>
                    <button onclick="pinApp.deleteCanvas('${canvas.id}')" class="btn btn-danger">删除</button>
                </div>
            `;
            container.appendChild(canvasDiv);
        });
    }
    
    renderCanvasEditor(canvas) {
        const editorContainer = document.getElementById('canvas-editor-container');
        const viewport = document.getElementById('canvas-viewport');
        if (!editorContainer || !viewport) return;
        
        // Clear existing content
        viewport.innerHTML = '';
        
        // Create canvas background
        const canvasBackground = document.createElement('div');
        canvasBackground.className = 'canvas-background';
        canvasBackground.style.width = '600px';
        canvasBackground.style.height = '448px';
        canvasBackground.style.backgroundColor = this.getColorValue(canvas.background_color || 1); // WHITE
        canvasBackground.style.position = 'relative';
        canvasBackground.style.border = '1px solid #ccc';
        viewport.appendChild(canvasBackground);
        
        // Render elements
        if (canvas.elements) {
            canvas.elements.forEach(element => {
                this.renderCanvasElement(canvasBackground, element);
            });
        }
        
        // Update canvas info
        document.getElementById('canvas-name').textContent = canvas.name;
        document.getElementById('canvas-element-count').textContent = canvas.elements ? canvas.elements.length : 0;
    }
    
    renderCanvasElement(container, element) {
        const elementDiv = document.createElement('div');
        elementDiv.className = 'canvas-element';
        elementDiv.style.position = 'absolute';
        elementDiv.style.left = element.x + 'px';
        elementDiv.style.top = element.y + 'px';
        elementDiv.style.width = element.width + 'px';
        elementDiv.style.height = element.height + 'px';
        elementDiv.style.border = '1px dashed #007bff';
        elementDiv.style.cursor = 'pointer';
        elementDiv.dataset.elementId = element.id;
        
        if (!element.visible) {
            elementDiv.style.opacity = '0.5';
        }
        
        // Render based on element type
        switch (element.type) {
            case 0: // TEXT
                elementDiv.innerHTML = `
                    <div style="
                        color: ${this.getColorValue(element.props.color)};
                        font-size: ${element.props.font_size}px;
                        font-weight: ${element.props.bold ? 'bold' : 'normal'};
                        font-style: ${element.props.italic ? 'italic' : 'normal'};
                        text-align: ${this.getAlignValue(element.props.align)};
                        width: 100%;
                        height: 100%;
                        display: flex;
                        align-items: center;
                        padding: 4px;
                        box-sizing: border-box;
                    ">${element.props.text}</div>
                `;
                break;
            case 1: // IMAGE
                elementDiv.innerHTML = `
                    <div style="
                        width: 100%;
                        height: 100%;
                        background: linear-gradient(45deg, #f0f0f0 25%, transparent 25%, transparent 75%, #f0f0f0 75%), 
                                    linear-gradient(45deg, #f0f0f0 25%, transparent 25%, transparent 75%, #f0f0f0 75%);
                        background-size: 10px 10px;
                        background-position: 0 0, 5px 5px;
                        display: flex;
                        align-items: center;
                        justify-content: center;
                        font-size: 12px;
                        color: #666;
                    ">图片: ${element.props.image_id}</div>
                `;
                break;
            case 2: // RECT
                elementDiv.style.backgroundColor = this.getColorValue(element.props.fill_color);
                elementDiv.style.borderColor = this.getColorValue(element.props.border_color);
                if (!element.props.filled) {
                    elementDiv.style.backgroundColor = 'transparent';
                }
                break;
        }
        
        // Add click handler for selection
        elementDiv.onclick = (e) => {
            e.stopPropagation();
            this.selectElement(element.id);
        };
        
        container.appendChild(elementDiv);
    }
    
    selectElement(elementId) {
        // Remove previous selection
        document.querySelectorAll('.canvas-element.selected').forEach(el => {
            el.classList.remove('selected');
        });
        
        // Add selection to clicked element
        const element = document.querySelector(`[data-element-id="${elementId}"]`);
        if (element) {
            element.classList.add('selected');
            this.state.selectedElement = elementId;
        }
    }
    
    showCanvasEditor() {
        document.getElementById('main-content').style.display = 'none';
        document.getElementById('canvas-editor').style.display = 'block';
    }
    
    hideCanvasEditor() {
        document.getElementById('canvas-editor').style.display = 'none';
        document.getElementById('main-content').style.display = 'block';
    }
    
    getColorValue(colorIndex) {
        const colors = ['#000000', '#FFFFFF', '#FF0000', '#FFFF00', '#0000FF', '#00FF00', '#FF8000'];
        return colors[colorIndex] || '#000000';
    }
    
    getAlignValue(alignIndex) {
        const aligns = ['left', 'center', 'right'];
        return aligns[alignIndex] || 'left';
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
    
    // Canvas API methods
    async getCanvases() {
        return this.request('/api/canvas');
    }
    
    async createCanvas(id, name) {
        return this.request('/api/canvas', {
            method: 'POST',
            body: JSON.stringify({ id, name })
        });
    }
    
    async getCanvas(canvasId) {
        return this.request(`/api/canvas/get?id=${encodeURIComponent(canvasId)}`);
    }
    
    async updateCanvas(canvas) {
        return this.request('/api/canvas/update', {
            method: 'PUT',
            body: JSON.stringify(canvas)
        });
    }
    
    async deleteCanvas(canvasId) {
        return this.request(`/api/canvas/delete?id=${encodeURIComponent(canvasId)}`, {
            method: 'DELETE'
        });
    }
    
    async displayCanvas(canvasId) {
        return this.request('/api/canvas/display', {
            method: 'POST',
            body: JSON.stringify({ canvas_id: canvasId })
        });
    }
    
    async addCanvasElement(canvasId, element) {
        return this.request('/api/canvas/element', {
            method: 'POST',
            body: JSON.stringify({ canvas_id: canvasId, element })
        });
    }
    
    async uploadImage(imageId, file) {
        const formData = new FormData();
        formData.append('image', file);
        
        return this.request(`/api/images?id=${encodeURIComponent(imageId)}`, {
            method: 'POST',
            body: formData,
            headers: {} // Let browser set Content-Type for FormData
        });
    }
}

// 初始化应用
document.addEventListener('DOMContentLoaded', () => {
    window.pinApp = new PinConfigApp();
});