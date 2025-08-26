# Pin WiFi 配置流程设计文档

## 概述

Pin设备的WiFi配置采用AP模式配网方案，用户通过连接设备热点，访问内置的PWA应用完成网络配置。这种方案无需额外的APP下载，用户体验友好。

## 配置流程设计

### 整体流程图

```
用户配置流程
┌─────────────────┐
│   设备开机      │
└─────┬───────────┘
      │
      ▼
┌─────────────────┐     ┌─────────────────┐
│ 检查WiFi配置    │────▶│   有配置信息    │
└─────┬───────────┘     └─────┬───────────┘
      │ 无配置                │
      ▼                       ▼
┌─────────────────┐     ┌─────────────────┐
│ 启动AP配网模式  │     │   连接WiFi     │
└─────┬───────────┘     └─────┬───────────┘
      │                       │ 连接失败
      ▼                       │
┌─────────────────┐           │
│ 显示配网提示    │◄──────────┘
└─────┬───────────┘
      │
      ▼
┌─────────────────┐
│ 用户连接热点    │
└─────┬───────────┘
      │
      ▼
┌─────────────────┐
│ 访问配置页面    │
└─────┬───────────┘
      │
      ▼
┌─────────────────┐
│ 扫描WiFi网络    │
└─────┬───────────┘
      │
      ▼
┌─────────────────┐
│ 选择并输入密码  │
└─────┬───────────┘
      │
      ▼
┌─────────────────┐     ┌─────────────────┐
│   发送配置      │────▶│   配置成功      │
└─────┬───────────┘     └─────┬───────────┘
      │ 配置失败              │
      ▼                       ▼
┌─────────────────┐     ┌─────────────────┐
│ 显示错误信息    │     │  保存配置信息   │
└─────┬───────────┘     └─────┬───────────┘
      │                       │
      └───────────────────────▼
                    ┌─────────────────┐
                    │   正常工作      │
                    └─────────────────┘
```

### 状态机设计

```c
// WiFi配置状态枚举
typedef enum {
    WIFI_CONFIG_STATE_IDLE,         // 空闲状态
    WIFI_CONFIG_STATE_CHECK_SAVED,  // 检查保存的配置
    WIFI_CONFIG_STATE_AP_MODE,      // AP模式
    WIFI_CONFIG_STATE_PORTAL_ACTIVE, // 配网门户活跃
    WIFI_CONFIG_STATE_CONNECTING,   // 正在连接
    WIFI_CONFIG_STATE_CONNECTED,    // 已连接
    WIFI_CONFIG_STATE_FAILED,       // 连接失败
    WIFI_CONFIG_STATE_TIMEOUT       // 配网超时
} pin_wifi_config_state_t;

// WiFi配置管理结构
typedef struct {
    pin_wifi_config_state_t state;          // 当前状态
    char ap_ssid[32];                       // AP模式SSID
    char ap_password[64];                   // AP模式密码 (可选)
    char target_ssid[32];                   // 目标WiFi SSID
    char target_password[64];               // 目标WiFi密码
    uint32_t config_timeout_ms;             // 配网超时时间
    uint32_t connect_timeout_ms;            // 连接超时时间
    uint32_t portal_start_time;             // 配网门户启动时间
    uint8_t retry_count;                    // 重试次数
    uint8_t max_retry;                      // 最大重试次数
    httpd_handle_t config_server;           // 配网web服务器
    EventGroupHandle_t wifi_event_group;    // WiFi事件组
    bool config_received;                   // 是否收到配置
    bool force_ap_mode;                     // 强制AP模式标志
} pin_wifi_config_t;
```

## 核心实现

### 主状态机实现

```c
// pin_wifi.c - WiFi配置主任务
static pin_wifi_config_t g_wifi_config = {0};
static const char* TAG = "PIN_WIFI";

void pin_wifi_config_task(void *pvParameters) {
    TickType_t last_wake_time = xTaskGetTickCount();
    
    ESP_LOGI(TAG, "WiFi configuration task started");
    
    while (1) {
        switch (g_wifi_config.state) {
            case WIFI_CONFIG_STATE_IDLE:
                wifi_handle_idle_state();
                break;
                
            case WIFI_CONFIG_STATE_CHECK_SAVED:
                wifi_handle_check_saved_state();
                break;
                
            case WIFI_CONFIG_STATE_AP_MODE:
                wifi_handle_ap_mode_state();
                break;
                
            case WIFI_CONFIG_STATE_PORTAL_ACTIVE:
                wifi_handle_portal_active_state();
                break;
                
            case WIFI_CONFIG_STATE_CONNECTING:
                wifi_handle_connecting_state();
                break;
                
            case WIFI_CONFIG_STATE_CONNECTED:
                wifi_handle_connected_state();
                break;
                
            case WIFI_CONFIG_STATE_FAILED:
                wifi_handle_failed_state();
                break;
                
            case WIFI_CONFIG_STATE_TIMEOUT:
                wifi_handle_timeout_state();
                break;
        }
        
        // 每秒执行一次状态机
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1000));
    }
}

// 空闲状态处理
static void wifi_handle_idle_state(void) {
    ESP_LOGI(TAG, "WiFi config idle, checking for saved configuration...");
    g_wifi_config.state = WIFI_CONFIG_STATE_CHECK_SAVED;
}

// 检查保存配置状态处理
static void wifi_handle_check_saved_state(void) {
    if (pin_wifi_has_saved_config() && !g_wifi_config.force_ap_mode) {
        // 有保存的配置，尝试连接
        pin_wifi_load_saved_config(g_wifi_config.target_ssid, g_wifi_config.target_password);
        ESP_LOGI(TAG, "Found saved WiFi config for SSID: %s", g_wifi_config.target_ssid);
        g_wifi_config.state = WIFI_CONFIG_STATE_CONNECTING;
    } else {
        // 没有配置或强制AP模式，启动配网
        ESP_LOGI(TAG, "No saved WiFi config found, starting AP mode");
        g_wifi_config.state = WIFI_CONFIG_STATE_AP_MODE;
    }
}

// AP模式状态处理
static void wifi_handle_ap_mode_state(void) {
    esp_err_t ret = pin_wifi_start_ap_mode();
    if (ret == ESP_OK) {
        ret = pin_wifi_start_config_portal();
        if (ret == ESP_OK) {
            g_wifi_config.portal_start_time = xTaskGetTickCount();
            g_wifi_config.state = WIFI_CONFIG_STATE_PORTAL_ACTIVE;
            
            // 在屏幕上显示配网信息
            pin_display_show_config_info(g_wifi_config.ap_ssid);
            ESP_LOGI(TAG, "WiFi config portal started, SSID: %s", g_wifi_config.ap_ssid);
        } else {
            ESP_LOGE(TAG, "Failed to start config portal");
            g_wifi_config.state = WIFI_CONFIG_STATE_FAILED;
        }
    } else {
        ESP_LOGE(TAG, "Failed to start AP mode");
        g_wifi_config.state = WIFI_CONFIG_STATE_FAILED;
    }
}

// 配网门户活跃状态处理
static void wifi_handle_portal_active_state(void) {
    uint32_t elapsed_time = (xTaskGetTickCount() - g_wifi_config.portal_start_time) * portTICK_PERIOD_MS;
    
    if (g_wifi_config.config_received) {
        // 收到配置，停止AP模式，尝试连接
        ESP_LOGI(TAG, "WiFi configuration received, attempting to connect...");
        pin_wifi_stop_config_portal();
        g_wifi_config.state = WIFI_CONFIG_STATE_CONNECTING;
    } else if (elapsed_time > g_wifi_config.config_timeout_ms) {
        // 配网超时
        ESP_LOGW(TAG, "WiFi configuration timeout after %d ms", elapsed_time);
        g_wifi_config.state = WIFI_CONFIG_STATE_TIMEOUT;
    }
    // 继续等待用户配置
}

// 连接状态处理
static void wifi_handle_connecting_state(void) {
    static uint32_t connect_start_time = 0;
    static bool connect_started = false;
    
    if (!connect_started) {
        connect_start_time = xTaskGetTickCount();
        connect_started = true;
        
        // 停止AP模式并切换到STA模式
        esp_wifi_set_mode(WIFI_MODE_STA);
        
        wifi_config_t sta_config = {0};
        strncpy((char*)sta_config.sta.ssid, g_wifi_config.target_ssid, sizeof(sta_config.sta.ssid));
        strncpy((char*)sta_config.sta.password, g_wifi_config.target_password, sizeof(sta_config.sta.password));
        
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        esp_wifi_connect();
        
        // 更新显示状态
        pin_display_show_connecting_status(g_wifi_config.target_ssid);
        ESP_LOGI(TAG, "Connecting to WiFi: %s", g_wifi_config.target_ssid);
    }
    
    uint32_t elapsed_time = (xTaskGetTickCount() - connect_start_time) * portTICK_PERIOD_MS;
    
    // 检查连接结果
    EventBits_t bits = xEventGroupWaitBits(g_wifi_config.wifi_event_group, 
                                          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                          pdTRUE, pdFALSE, 0);
    
    if (bits & WIFI_CONNECTED_BIT) {
        // 连接成功
        ESP_LOGI(TAG, "WiFi connected successfully");
        pin_wifi_save_config(g_wifi_config.target_ssid, g_wifi_config.target_password);
        g_wifi_config.retry_count = 0;
        connect_started = false;
        g_wifi_config.state = WIFI_CONFIG_STATE_CONNECTED;
    } else if ((bits & WIFI_FAIL_BIT) || (elapsed_time > g_wifi_config.connect_timeout_ms)) {
        // 连接失败或超时
        ESP_LOGW(TAG, "WiFi connection failed or timeout");
        connect_started = false;
        g_wifi_config.retry_count++;
        g_wifi_config.state = WIFI_CONFIG_STATE_FAILED;
    }
}

// 连接成功状态处理
static void wifi_handle_connected_state(void) {
    // 显示连接成功信息
    pin_display_show_connected_status(g_wifi_config.target_ssid);
    
    // 定期检查连接状态
    if (!pin_wifi_is_connected()) {
        ESP_LOGW(TAG, "WiFi connection lost, attempting to reconnect");
        g_wifi_config.state = WIFI_CONFIG_STATE_CONNECTING;
    }
}

// 连接失败状态处理
static void wifi_handle_failed_state(void) {
    if (g_wifi_config.retry_count < g_wifi_config.max_retry) {
        ESP_LOGI(TAG, "Retrying WiFi connection (%d/%d)", g_wifi_config.retry_count, g_wifi_config.max_retry);
        vTaskDelay(pdMS_TO_TICKS(5000));  // 等待5秒后重试
        g_wifi_config.state = WIFI_CONFIG_STATE_CONNECTING;
    } else {
        ESP_LOGE(TAG, "WiFi connection failed after %d retries, restarting config", g_wifi_config.max_retry);
        pin_display_show_failed_status("连接失败，重新配置");
        g_wifi_config.retry_count = 0;
        g_wifi_config.state = WIFI_CONFIG_STATE_AP_MODE;
    }
}

// 超时状态处理
static void wifi_handle_timeout_state(void) {
    ESP_LOGW(TAG, "WiFi configuration timeout, restarting AP mode");
    pin_display_show_timeout_status();
    pin_wifi_stop_config_portal();
    vTaskDelay(pdMS_TO_TICKS(3000));  // 显示超时信息3秒
    g_wifi_config.state = WIFI_CONFIG_STATE_AP_MODE;
}
```

### AP模式和热点配置

```c
// 启动AP模式
esp_err_t pin_wifi_start_ap_mode(void) {
    // 生成唯一的AP SSID
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    snprintf(g_wifi_config.ap_ssid, sizeof(g_wifi_config.ap_ssid), 
             "Pin-Device-%02X%02X", mac[4], mac[5]);
    
    // 配置AP参数
    wifi_config_t ap_config = {
        .ap = {
            .ssid_len = strlen(g_wifi_config.ap_ssid),
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,  // 开放网络，便于连接
            .beacon_interval = 100,
        }
    };
    
    strcpy((char*)ap_config.ap.ssid, g_wifi_config.ap_ssid);
    
    // 设置WiFi模式为AP
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    
    // 启动WiFi
    esp_err_t ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi AP: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 配置AP的IP地址
    esp_netif_t* ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_ip_info_t ip_info = {
        .ip = { .addr = ESP_IP4TOADDR(192, 168, 4, 1) },
        .gw = { .addr = ESP_IP4TOADDR(192, 168, 4, 1) },
        .netmask = { .addr = ESP_IP4TOADDR(255, 255, 255, 0) }
    };
    
    ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));
    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip_info));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));
    
    ESP_LOGI(TAG, "WiFi AP started. SSID: %s, IP: 192.168.4.1", g_wifi_config.ap_ssid);
    return ESP_OK;
}

// DNS服务器实现 (强制门户功能)
static void pin_dns_server_task(void *pvParameters) {
    struct sockaddr_in server_addr;
    int sock;
    
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create DNS socket");
        vTaskDelete(NULL);
        return;
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(53);
    
    if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind DNS socket");
        close(sock);
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "DNS server started on port 53");
    
    uint8_t rx_buffer[512];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    while (1) {
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer), 0, 
                          (struct sockaddr*)&client_addr, &client_len);
        
        if (len > 0) {
            // 构建DNS响应，将所有查询都指向AP的IP地址
            uint8_t response[512];
            int response_len = pin_build_dns_response(rx_buffer, len, response, sizeof(response));
            
            if (response_len > 0) {
                sendto(sock, response, response_len, 0, 
                      (struct sockaddr*)&client_addr, client_len);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// 构建DNS响应 (强制门户)
static int pin_build_dns_response(const uint8_t* query, int query_len, 
                                 uint8_t* response, int max_len) {
    if (query_len < 12 || max_len < query_len + 16) {
        return 0;
    }
    
    // 复制查询到响应
    memcpy(response, query, query_len);
    
    // 设置响应标志
    response[2] |= 0x80;  // QR=1 (Response)
    response[3] |= 0x80;  // RA=1 (Recursion Available)
    
    // 添加答案记录
    int pos = query_len;
    
    // Name (pointer to question)
    response[pos++] = 0xC0;
    response[pos++] = 0x0C;
    
    // Type (A record)
    response[pos++] = 0x00;
    response[pos++] = 0x01;
    
    // Class (IN)
    response[pos++] = 0x00;
    response[pos++] = 0x01;
    
    // TTL (60 seconds)
    response[pos++] = 0x00;
    response[pos++] = 0x00;
    response[pos++] = 0x00;
    response[pos++] = 0x3C;
    
    // Data length (4 bytes for IPv4)
    response[pos++] = 0x00;
    response[pos++] = 0x04;
    
    // IP address (192.168.4.1)
    response[pos++] = 192;
    response[pos++] = 168;
    response[pos++] = 4;
    response[pos++] = 1;
    
    // 更新答案记录数
    response[7] = 1;
    
    return pos;
}
```

### 配网Web服务器

```c
// 启动配网门户
esp_err_t pin_wifi_start_config_portal(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 15;
    config.stack_size = 8192;
    
    esp_err_t ret = httpd_start(&g_wifi_config.config_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start config portal server");
        return ret;
    }
    
    // 注册路由处理器
    
    // 根路径 - 重定向到配置页面
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = config_portal_root_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(g_wifi_config.config_server, &root_uri);
    
    // 配置页面
    httpd_uri_t config_uri = {
        .uri = "/config",
        .method = HTTP_GET,
        .handler = config_portal_page_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(g_wifi_config.config_server, &config_uri);
    
    // WiFi扫描API
    httpd_uri_t scan_uri = {
        .uri = "/api/wifi/scan",
        .method = HTTP_GET,
        .handler = wifi_scan_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(g_wifi_config.config_server, &scan_uri);
    
    // WiFi连接API
    httpd_uri_t connect_uri = {
        .uri = "/api/wifi/connect",
        .method = HTTP_POST,
        .handler = wifi_connect_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(g_wifi_config.config_server, &connect_uri);
    
    // 状态查询API
    httpd_uri_t status_uri = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = config_status_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(g_wifi_config.config_server, &status_uri);
    
    // 强制门户 - 处理所有其他请求
    httpd_uri_t portal_uri = {
        .uri = "*",
        .method = HTTP_GET,
        .handler = captive_portal_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(g_wifi_config.config_server, &portal_uri);
    
    // 启动DNS服务器用于强制门户
    xTaskCreate(pin_dns_server_task, "dns_server", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "WiFi config portal started on http://192.168.4.1");
    return ESP_OK;
}

// WiFi扫描处理器
static esp_err_t wifi_scan_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "WiFi scan requested");
    
    // 执行WiFi扫描
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 300
    };
    
    esp_err_t ret = esp_wifi_scan_start(&scan_config, true);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Scan failed");
        return ESP_FAIL;
    }
    
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    if (ap_count == 0) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"networks\":[]}");
        return ESP_OK;
    }
    
    wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!ap_records) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory error");
        return ESP_FAIL;
    }
    
    esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    
    // 构建JSON响应
    cJSON *json = cJSON_CreateObject();
    cJSON *networks = cJSON_CreateArray();
    
    for (int i = 0; i < ap_count; i++) {
        cJSON *network = cJSON_CreateObject();
        cJSON_AddStringToObject(network, "ssid", (char*)ap_records[i].ssid);
        cJSON_AddNumberToObject(network, "rssi", ap_records[i].rssi);
        cJSON_AddNumberToObject(network, "auth", ap_records[i].authmode);
        cJSON_AddNumberToObject(network, "channel", ap_records[i].primary);
        cJSON_AddItemToArray(networks, network);
    }
    
    cJSON_AddItemToObject(json, "networks", networks);
    
    const char *json_string = cJSON_Print(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, json_string);
    
    free((void*)json_string);
    cJSON_Delete(json);
    free(ap_records);
    
    ESP_LOGI(TAG, "WiFi scan completed, found %d networks", ap_count);
    return ESP_OK;
}

// WiFi连接处理器
static esp_err_t wifi_connect_handler(httpd_req_t *req) {
    char content[256];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        return ESP_FAIL;
    }
    content[ret] = '\0';
    
    // 解析JSON请求
    cJSON *json = cJSON_Parse(content);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON *ssid_json = cJSON_GetObjectItem(json, "ssid");
    cJSON *password_json = cJSON_GetObjectItem(json, "password");
    
    if (!ssid_json || !cJSON_IsString(ssid_json)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing SSID");
        return ESP_FAIL;
    }
    
    // 保存配置信息
    strncpy(g_wifi_config.target_ssid, ssid_json->valuestring, sizeof(g_wifi_config.target_ssid) - 1);
    
    if (password_json && cJSON_IsString(password_json)) {
        strncpy(g_wifi_config.target_password, password_json->valuestring, sizeof(g_wifi_config.target_password) - 1);
    } else {
        g_wifi_config.target_password[0] = '\0';
    }
    
    cJSON_Delete(json);
    
    // 标记收到配置
    g_wifi_config.config_received = true;
    
    // 返回成功响应
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "message", "Configuration received, connecting...");
    
    const char *response_string = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, response_string);
    
    free((void*)response_string);
    cJSON_Delete(response);
    
    ESP_LOGI(TAG, "WiFi configuration received: SSID=%s", g_wifi_config.target_ssid);
    return ESP_OK;
}

// 强制门户处理器
static esp_err_t captive_portal_handler(httpd_req_t *req) {
    // 重定向所有请求到配置页面
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/config");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}
```

### 显示系统集成

```c
// 显示配网信息
void pin_display_show_config_info(const char* ap_ssid) {
    pin_display_clear(PIN_COLOR_WHITE);
    
    // 显示标题
    pin_display_draw_text(50, 50, "Pin 配置模式", PIN_FONT_LARGE, PIN_COLOR_BLACK);
    
    // 显示连接步骤
    char step1[64];
    snprintf(step1, sizeof(step1), "1. 连接WiFi: %s", ap_ssid);
    pin_display_draw_text(20, 120, step1, PIN_FONT_MEDIUM, PIN_COLOR_BLACK);
    
    pin_display_draw_text(20, 150, "2. 打开 192.168.4.1", PIN_FONT_MEDIUM, PIN_COLOR_BLACK);
    pin_display_draw_text(20, 180, "3. 按照引导配置", PIN_FONT_MEDIUM, PIN_COLOR_BLACK);
    
    // 显示二维码 (可选)
    char qr_url[128];
    snprintf(qr_url, sizeof(qr_url), "http://192.168.4.1/config");
    pin_display_draw_qr_code(450, 120, qr_url, 100);
    
    pin_display_refresh(PIN_REFRESH_FULL);
}

// 显示连接状态
void pin_display_show_connecting_status(const char* ssid) {
    pin_display_clear(PIN_COLOR_WHITE);
    
    pin_display_draw_text(100, 100, "正在连接...", PIN_FONT_LARGE, PIN_COLOR_BLACK);
    
    char status[64];
    snprintf(status, sizeof(status), "网络: %s", ssid);
    pin_display_draw_text(50, 150, status, PIN_FONT_MEDIUM, PIN_COLOR_BLUE);
    
    // 绘制加载动画
    pin_display_draw_loading_animation(300, 200, 30);
    
    pin_display_refresh(PIN_REFRESH_PARTIAL);
}

// 显示连接成功状态
void pin_display_show_connected_status(const char* ssid) {
    pin_display_clear(PIN_COLOR_WHITE);
    
    pin_display_draw_text(100, 100, "连接成功!", PIN_FONT_LARGE, PIN_COLOR_GREEN);
    
    char status[64];
    snprintf(status, sizeof(status), "网络: %s", ssid);
    pin_display_draw_text(50, 150, status, PIN_FONT_MEDIUM, PIN_COLOR_BLACK);
    
    // 显示WiFi信号强度图标
    pin_display_draw_wifi_icon(300, 100, pin_wifi_get_rssi(), PIN_COLOR_GREEN);
    
    pin_display_refresh(PIN_REFRESH_FULL);
}

// 显示连接失败状态
void pin_display_show_failed_status(const char* message) {
    pin_display_clear(PIN_COLOR_WHITE);
    
    pin_display_draw_text(100, 100, "连接失败", PIN_FONT_LARGE, PIN_COLOR_RED);
    pin_display_draw_text(50, 150, message, PIN_FONT_MEDIUM, PIN_COLOR_BLACK);
    pin_display_draw_text(50, 200, "请重新配置", PIN_FONT_MEDIUM, PIN_COLOR_BLUE);
    
    pin_display_refresh(PIN_REFRESH_FULL);
}
```

## 配置数据持久化

### NVS存储实现

```c
// WiFi配置持久化
#define PIN_WIFI_NAMESPACE "pin_wifi"
#define PIN_WIFI_SSID_KEY "ssid"
#define PIN_WIFI_PASSWORD_KEY "password"
#define PIN_WIFI_CONFIG_VERSION_KEY "version"

// 保存WiFi配置
esp_err_t pin_wifi_save_config(const char* ssid, const char* password) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(PIN_WIFI_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 保存配置版本
    ret = nvs_set_u8(nvs_handle, PIN_WIFI_CONFIG_VERSION_KEY, 1);
    if (ret != ESP_OK) goto cleanup;
    
    // 保存SSID
    ret = nvs_set_str(nvs_handle, PIN_WIFI_SSID_KEY, ssid);
    if (ret != ESP_OK) goto cleanup;
    
    // 保存密码 (加密存储)
    if (password && strlen(password) > 0) {
        char encrypted_password[128];
        pin_crypto_encrypt(password, encrypted_password, sizeof(encrypted_password));
        ret = nvs_set_str(nvs_handle, PIN_WIFI_PASSWORD_KEY, encrypted_password);
    } else {
        ret = nvs_erase_key(nvs_handle, PIN_WIFI_PASSWORD_KEY);
    }
    
    if (ret != ESP_OK) goto cleanup;
    
    ret = nvs_commit(nvs_handle);
    
cleanup:
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi configuration saved successfully");
    } else {
        ESP_LOGE(TAG, "Failed to save WiFi configuration: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

// 加载WiFi配置
esp_err_t pin_wifi_load_saved_config(char* ssid, char* password) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(PIN_WIFI_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    size_t required_size = 32;
    ret = nvs_get_str(nvs_handle, PIN_WIFI_SSID_KEY, ssid, &required_size);
    if (ret != ESP_OK) goto cleanup;
    
    // 读取密码
    char encrypted_password[128];
    required_size = sizeof(encrypted_password);
    ret = nvs_get_str(nvs_handle, PIN_WIFI_PASSWORD_KEY, encrypted_password, &required_size);
    if (ret == ESP_OK) {
        pin_crypto_decrypt(encrypted_password, password, 64);
    } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
        password[0] = '\0';  // 没有密码
        ret = ESP_OK;
    }
    
cleanup:
    nvs_close(nvs_handle);
    return ret;
}

// 检查是否有保存的配置
bool pin_wifi_has_saved_config(void) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(PIN_WIFI_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        return false;
    }
    
    size_t required_size = 0;
    ret = nvs_get_str(nvs_handle, PIN_WIFI_SSID_KEY, NULL, &required_size);
    nvs_close(nvs_handle);
    
    return (ret == ESP_OK && required_size > 1);
}

// 清除WiFi配置
esp_err_t pin_wifi_clear_config(void) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(PIN_WIFI_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    nvs_erase_all(nvs_handle);
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "WiFi configuration cleared");
    return ret;
}
```

## 安全考虑

### 密码加密存储

```c
// 简单的XOR加密 (可替换为更强的加密算法)
static const char* ENCRYPT_KEY = "PinDevice2024";

void pin_crypto_encrypt(const char* plaintext, char* encrypted, size_t max_len) {
    size_t key_len = strlen(ENCRYPT_KEY);
    size_t text_len = strlen(plaintext);
    
    for (size_t i = 0; i < text_len && i < max_len - 1; i++) {
        encrypted[i] = plaintext[i] ^ ENCRYPT_KEY[i % key_len];
    }
    encrypted[text_len] = '\0';
    
    // Base64编码
    mbedtls_base64_encode((unsigned char*)encrypted, max_len, &text_len,
                         (const unsigned char*)encrypted, text_len);
}

void pin_crypto_decrypt(const char* encrypted, char* plaintext, size_t max_len) {
    size_t key_len = strlen(ENCRYPT_KEY);
    size_t decoded_len;
    
    // Base64解码
    mbedtls_base64_decode((unsigned char*)plaintext, max_len, &decoded_len,
                         (const unsigned char*)encrypted, strlen(encrypted));
    
    // XOR解密
    for (size_t i = 0; i < decoded_len; i++) {
        plaintext[i] ^= ENCRYPT_KEY[i % key_len];
    }
    plaintext[decoded_len] = '\0';
}
```

### 访问控制

```c
// 配网安全配置
typedef struct {
    uint32_t max_config_attempts;      // 最大配置尝试次数
    uint32_t config_attempt_window;    // 尝试窗口时间(秒)
    uint32_t lockout_duration;         // 锁定持续时间(秒)
    bool enable_rate_limiting;         // 启用频率限制
} pin_wifi_security_config_t;

static pin_wifi_security_config_t g_security_config = {
    .max_config_attempts = 5,
    .config_attempt_window = 300,      // 5分钟
    .lockout_duration = 600,           // 10分钟
    .enable_rate_limiting = true
};

// 频率限制检查
static bool pin_wifi_check_rate_limit(const char* client_ip) {
    if (!g_security_config.enable_rate_limiting) {
        return true;
    }
    
    // TODO: 实现IP地址基础的频率限制
    // 记录每个IP的请求次数和时间
    
    return true;  // 简化实现
}
```

## 用户体验优化

### 响应式设计

```css
/* 移动端优化 */
@media (max-width: 480px) {
    .config-container {
        padding: 16px;
        margin: 0;
    }
    
    .wifi-list {
        font-size: 14px;
    }
    
    .btn {
        height: 48px;
        font-size: 16px;
    }
}

/* 深色模式支持 */
@media (prefers-color-scheme: dark) {
    :root {
        --bg-color: #121212;
        --surface-color: #1e1e1e;
        --text-primary: #ffffff;
        --text-secondary: #b3b3b3;
    }
}
```

### 多语言支持

```javascript
// 国际化支持
const i18n = {
    'zh-CN': {
        'config_title': 'Pin 设备配置',
        'connect_wifi': '连接 WiFi',
        'scan_networks': '扫描网络',
        'connecting': '连接中...',
        'connected': '已连接',
        'connection_failed': '连接失败'
    },
    'en-US': {
        'config_title': 'Pin Device Configuration',
        'connect_wifi': 'Connect WiFi',
        'scan_networks': 'Scan Networks', 
        'connecting': 'Connecting...',
        'connected': 'Connected',
        'connection_failed': 'Connection Failed'
    }
};

function t(key) {
    const lang = navigator.language || 'en-US';
    return i18n[lang] && i18n[lang][key] ? i18n[lang][key] : key;
}
```

## 调试和诊断

### 日志系统

```c
// WiFi配置日志记录
#define PIN_WIFI_LOG_LEVEL ESP_LOG_INFO

void pin_wifi_log_state_change(pin_wifi_config_state_t old_state, pin_wifi_config_state_t new_state) {
    const char* state_names[] = {
        "IDLE", "CHECK_SAVED", "AP_MODE", "PORTAL_ACTIVE", 
        "CONNECTING", "CONNECTED", "FAILED", "TIMEOUT"
    };
    
    ESP_LOGI(TAG, "State change: %s -> %s", 
             state_names[old_state], state_names[new_state]);
    
    // 记录到文件或发送到远程日志服务器
    pin_log_write_to_file("wifi_state_change", state_names[old_state], state_names[new_state]);
}

// 网络诊断信息收集
void pin_wifi_collect_diagnostic_info(void) {
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        ESP_LOGI(TAG, "Connected AP: SSID=%s, RSSI=%d, Channel=%d", 
                 ap_info.ssid, ap_info.rssi, ap_info.primary);
    }
    
    tcpip_adapter_ip_info_t ip_info;
    if (tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info) == ESP_OK) {
        ESP_LOGI(TAG, "IP Info: IP=" IPSTR ", Gateway=" IPSTR ", Netmask=" IPSTR,
                 IP2STR(&ip_info.ip), IP2STR(&ip_info.gw), IP2STR(&ip_info.netmask));
    }
}
```

## 性能监控

### 配网性能指标

```c
// 配网性能统计
typedef struct {
    uint32_t total_config_attempts;    // 总配网次数
    uint32_t successful_configs;       // 成功配网次数
    uint32_t average_config_time_ms;   // 平均配网时间
    uint32_t average_connect_time_ms;  // 平均连接时间
    uint32_t portal_access_count;      // 门户访问次数
    uint32_t scan_request_count;       // 扫描请求次数
} pin_wifi_stats_t;

static pin_wifi_stats_t g_wifi_stats = {0};

void pin_wifi_update_stats(void) {
    // 更新统计信息
    g_wifi_stats.total_config_attempts++;
    
    // 计算平均时间等指标
    // ...
}

void pin_wifi_print_stats(void) {
    ESP_LOGI(TAG, "=== WiFi Configuration Statistics ===");
    ESP_LOGI(TAG, "Total attempts: %d", g_wifi_stats.total_config_attempts);
    ESP_LOGI(TAG, "Success rate: %.1f%%", 
             (float)g_wifi_stats.successful_configs / g_wifi_stats.total_config_attempts * 100);
    ESP_LOGI(TAG, "Average config time: %d ms", g_wifi_stats.average_config_time_ms);
    ESP_LOGI(TAG, "Average connect time: %d ms", g_wifi_stats.average_connect_time_ms);
}
```

Pin的WiFi配置流程通过AP模式配网、PWA应用界面、强制门户等技术的组合，为用户提供了简单、可靠、用户友好的网络配置体验，同时保证了系统的安全性和稳定性。