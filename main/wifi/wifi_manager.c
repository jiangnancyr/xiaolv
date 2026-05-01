// main/network/wifi_manager.c
#include "wifi_manager.h"
#include "logger/logger.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include <string.h>
#define TAG "WIFI"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_RETRY_BIT     BIT2

static EventGroupHandle_t g_wifi_event_group;
static int g_retry_count = 0;
static bool g_auto_reconnect = true;
static wifi_state_t g_wifi_state = WIFI_STATE_DISCONNECTED;
static wifi_callback_t g_user_callback = NULL;
static void *g_user_callback_arg = NULL;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                g_wifi_state = WIFI_STATE_CONNECTING;
                LOG_I(TAG, "WiFi connecting...");
                break;
                
            case WIFI_EVENT_STA_DISCONNECTED:
                g_wifi_state = WIFI_STATE_DISCONNECTED;
                LOG_W(TAG, "WiFi disconnected");
                if (g_user_callback) {
                    g_user_callback(WIFI_STATE_DISCONNECTED, g_user_callback_arg);
                }
                
                if (g_auto_reconnect && g_retry_count < 5) {
                    g_retry_count++;
                    LOG_I(TAG, "Reconnecting... (attempt %d/5)", g_retry_count);
                    esp_wifi_connect();
                } else {
                    xEventGroupSetBits(g_wifi_event_group, WIFI_FAIL_BIT);
                }
                break;
                
            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            char ip_str[16];
            esp_ip4addr_ntoa(&event->ip_info.ip, ip_str, sizeof(ip_str));
            
            g_wifi_state = WIFI_STATE_GOT_IP;
            g_retry_count = 0;
            
            LOG_I(TAG, "Got IP: %s", ip_str);
            
            if (g_user_callback) {
                g_user_callback(WIFI_STATE_GOT_IP, g_user_callback_arg);
            }
            
            xEventGroupSetBits(g_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

esp_err_t wifi_manager_init(void)
{
    // 初始化网络接�?
    esp_err_t ret = esp_netif_init(); if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) return ret;
    ret = esp_event_loop_create_default(); if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) return ret;
    
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // 注册事件处理
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                                &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                                &wifi_event_handler, NULL));
    
    g_wifi_event_group = xEventGroupCreate();
    
    LOG_I(TAG, "WiFi manager initialized");
    return ESP_OK;
}

esp_err_t wifi_connect(my_wifi_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;
    
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    
    strncpy((char*)wifi_config.sta.ssid, config->ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, config->password, sizeof(wifi_config.sta.password));
    
    // 保存用户回调
    g_user_callback = config->callback;
    g_user_callback_arg = config->callback_arg;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // WIFI_MODE_APSTA
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    LOG_I(TAG, "Connecting to WiFi: %s", config->ssid);
    
    // 等待连接（带超时�?
    EventBits_t bits = xEventGroupWaitBits(g_wifi_event_group,
                                            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                            pdFALSE,
                                            pdFALSE,
                                            pdMS_TO_TICKS(10000));
    
    if (bits & WIFI_CONNECTED_BIT) {
        LOG_I(TAG, "Connected to WiFi successfully");
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        LOG_E(TAG, "Failed to connect to WiFi");
        return ESP_FAIL;
    } else {
        LOG_E(TAG, "WiFi connection timeout");
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t wifi_disconnect(void)
{
    g_auto_reconnect = false;
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_stop());
    
    g_wifi_state = WIFI_STATE_DISCONNECTED;
    LOG_I(TAG, "WiFi disconnected");
    
    return ESP_OK;
}

wifi_state_t wifi_get_state(void)
{
    return g_wifi_state;
}

esp_err_t wifi_get_ip(char *ip_buffer, size_t buffer_size)
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) return ESP_FAIL;
    
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
        return ESP_FAIL;
    }
    
    esp_ip4addr_ntoa(&ip_info.ip, ip_buffer, buffer_size);
    return ESP_OK;
}

esp_err_t wifi_scan(wifi_ap_record_t **ap_records, uint16_t *count)
{
    esp_wifi_scan_start(NULL, true);
    esp_wifi_scan_get_ap_num(count);
    *ap_records = malloc(sizeof(wifi_ap_record_t) * (*count));
    if (!*ap_records) return ESP_ERR_NO_MEM;
    
    esp_wifi_scan_get_ap_records(count, *ap_records);
    return ESP_OK;
}

void wifi_enable_auto_reconnect(bool enable)
{
    g_auto_reconnect = enable;
    LOG_I(TAG, "Auto reconnect %s", enable ? "enabled" : "disabled");
}
