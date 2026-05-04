#include "siliconflow_tts.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_tls.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "task_flow_config.h"  // 添加任务配置头文件
#include "esp_crt_bundle.h"
#include "workflow.h"
#include "logger.h"
#include "example_config.h"

static const char *TAG = "SILICONFLOW_TTS";
#define BOUNDARY "----WebKitFormBoundary7MA4YWxkTrZu0gW"

// 如果没有cJSON，手动构建JSON
static char* build_json_payload(const char *text, const char *voice, const char *model) {
    // 手动构建JSON字符串
    char *json = (char*)malloc(512);
    if (!json) return NULL;

    snprintf(json, 512,
        "{"
        "\"model\":\"%s\","
        "\"input\":\"%s\","
        "\"voice\":\"%s\","
        "\"sample_rate\":16000,"
        "\"response_format\":\"pcm\","
        "\"stream\":true"
        "}",
        model, text, voice);

    return json;
}

// HTTP事件处理器
static esp_err_t siliconflow_tts_event_handler(esp_http_client_event_t *evt) {
    siliconflow_tts_config_t *config = (siliconflow_tts_config_t *)evt->user_data;
    static uint8_t *buffer = NULL;
    static size_t buffer_size = 0;
    static size_t data_received = 0;

    switch(evt->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "Connected to TTS server");
            // 初始化缓冲区
            buffer_size = 1024 * 1024;  // 1MB初始缓冲区
            buffer = (uint8_t *)malloc(buffer_size);
            if (!buffer) {
                ESP_LOGE(TAG, "Failed to allocate audio buffer");
                return ESP_ERR_NO_MEM;
            }
            data_received = 0;
            break;

        case HTTP_EVENT_ON_DATA:
            if (!evt->data || evt->data_len <= 0 || !buffer) break;

            // 检查缓冲区是否需要扩展
            if (data_received + evt->data_len > buffer_size) {
                size_t new_size = buffer_size * 2;
                while (new_size < data_received + evt->data_len) {
                    new_size *= 2;
                }
                uint8_t *new_buffer = (uint8_t *)realloc(buffer, new_size);
                if (!new_buffer) {
                    ESP_LOGE(TAG, "Failed to reallocate audio buffer");
                    free(buffer);
                    buffer = NULL;
                    return ESP_ERR_NO_MEM;
                }
                buffer = new_buffer;
                buffer_size = new_size;
            }

            // 复制数据到缓冲区
            memcpy(buffer + data_received, evt->data, evt->data_len);
            data_received += evt->data_len;
            break;

        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Disconnected from TTS server");
            if (config) {
                config->audio_buffer = buffer;
                config->audio_size = data_received;
                ESP_LOGI(TAG, "Audio received: %zu bytes", data_received);
            } else {
                free(buffer);
            }
            buffer = NULL;
            buffer_size = 0;
            data_received = 0;
            break;

        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "TTS request finished");
            break;

        default:
            break;
    }
    return ESP_OK;
}

esp_err_t siliconflow_tts_task(struct wf_runtime *rt, uint8_t self_task_id, void *arg) {
    siliconflow_tts_config_t *config = (siliconflow_tts_config_t *)arg;
    if (!config || !config->api_key) {
        ESP_LOGE(TAG, "Invalid TTS configuration");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;
    wf_message_t msg = {0};
    ret = wf_recv(rt, self_task_id, &msg, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to receive message for TTS task: %s", esp_err_to_name(ret));
        return ret;
    }
    // 构建JSON payload
    char *json_payload = build_json_payload(msg.ptr,
                                          config->voice ? config->voice : AI_TTS_VOICE,
                                          config->model ? config->model : AI_TTS_MODEL);
    if (!json_payload) {
        ESP_LOGE(TAG, "Failed to build JSON payload");
        return ESP_ERR_NO_MEM;
    }

    // 配置HTTP客户端
    esp_http_client_config_t http_config = {
        .url = AI_TTS_API_URL,
        .method = HTTP_METHOD_POST,
        .event_handler = siliconflow_tts_event_handler,
        .user_data = config,  // 传递配置给事件处理器
        .timeout_ms = 5000,
        .keep_alive_enable = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_idle = HTTP_KEEP_ALIVE_TIME,        // 空闲5秒后关闭连接
        .keep_alive_interval = 2,    // 2秒探测间隔
        .keep_alive_count = 3,       // 最多3次探测
        .disable_auto_redirect = true,  // 避免重定向导致状态混乱
        .buffer_size = 4096,         // 缓冲区大小
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(json_payload);
        return ESP_FAIL;
    }

    // 设置headers
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "%s", config->api_key ? config->api_key : AI_TTS_API_KEY);
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_header(client, "Content-Type", "application/json");

    // 设置POST数据
    ESP_LOGI(TAG, "Sending TTS request: %s", json_payload);
    ret = esp_http_client_set_post_field(client, json_payload, strlen(json_payload));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set POST field");
        goto cleanup;
    }
    int retry_count = 0;
    const int max_retries = 5;
    do {
        ret = esp_http_client_perform(client);
        if (ret == ESP_OK) {
            break;
        }
        retry_count++;
        if (retry_count < max_retries) {
            ESP_LOGW(TAG, "TTS HTTP request failed (attempt %d/%d), retrying...", retry_count, max_retries);
            vTaskDelay(pdMS_TO_TICKS(1000));  // 等待1秒后重试
        }
    } while (retry_count < max_retries);
    free(json_payload);
    json_payload = NULL;
    // 发送请求并等待响应
    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "TTS HTTP Status = %d", status_code);
    esp_http_client_cleanup(client);  // 清理客户端，触发事件处理器的断开事件
    client = NULL;  // 避免后续误用
    if (status_code != 200) {
        ESP_LOGE(TAG, "TTS request failed with status %d", status_code);
        ret = ESP_FAIL;
    } else {
        while(true) {
            size_t buffered_len = config->audio_size;
            // 等待数据接收完成，事件处理器会处理数据
            vTaskDelay(pdMS_TO_TICKS(100));
            if (buffered_len > 0 && buffered_len == config->audio_size && config->audio_buffer) {
                ESP_LOGI(TAG, "Audio data received: %zu bytes", buffered_len);
                ret = wf_send(rt, self_task_id, AUDIO_WF_TASK_PLAYBACK, AUDIO_PLAYER_MSG_AUDIO, buffered_len, (config->audio_buffer), portMAX_DELAY);
                if (ret != ESP_OK) {    
                    ESP_LOGE(TAG, "Failed to send audio data to player task: %s", esp_err_to_name(ret));
                }
                return ret;
            }
        }
    }

cleanup:
    if (client) {
        esp_http_client_cleanup(client);
        client = NULL;
    }
    if (json_payload) {
        free(json_payload);
        json_payload = NULL;
    }
    return ret;
}