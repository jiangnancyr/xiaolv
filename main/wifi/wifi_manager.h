// main/network/wifi_manager.h
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include "esp_netif.h"
#include <esp_wifi_types_generic.h>

typedef enum {
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_GOT_IP,
    WIFI_STATE_ERROR
} wifi_state_t;

typedef void (*wifi_callback_t)(wifi_state_t state, void *arg);

// WiFi配置
typedef struct {
    char ssid[32];
    char password[64];
    wifi_callback_t callback;
    void *callback_arg;
    int max_retry;
} my_wifi_config_t;

// 初始化WiFi管理器
esp_err_t wifi_manager_init(void);

// 连接WiFi
esp_err_t wifi_connect(my_wifi_config_t *config);

// 断开WiFi
esp_err_t wifi_disconnect(void);

// 获取WiFi状态
wifi_state_t wifi_get_state(void);

// 获取IP地址
esp_err_t wifi_get_ip(char *ip_buffer, size_t buffer_size);

// 扫描WiFi网络
esp_err_t wifi_scan(wifi_ap_record_t **ap_records, uint16_t *count);

// 自动重连
void wifi_enable_auto_reconnect(bool enable);

#endif // WIFI_MANAGER_H