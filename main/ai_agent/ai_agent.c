#include "ai_agent.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "example_config.h"
#include <time.h>
#include "logger.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "ai_agent";

static ai_agent_config_t s_config;
static bool s_initialized;
static bool s_started;
static esp_http_client_handle_t g_ai_agent_http_config = NULL;

typedef struct {
    char *buffer;
    size_t buffer_size;
    size_t data_len;
    bool overflow;
} ai_agent_http_response_t;

#if AI_AGENT_ENABLE_MEMORY
typedef struct {
    char *role;
    char *content;
    uint32_t timestamp;
} ai_agent_message_t;

typedef struct {
    char summary[AI_AGENT_MEMORY_MAX_SUMMARY_LEN];
    uint32_t last_update_time;
    uint32_t message_count;
} ai_agent_memory_t;

static ai_agent_message_t *s_history = NULL;
static size_t s_history_count = 0;
static ai_agent_memory_t s_memory = {0};

/* ========== NVS 持久化函数 ========== */
static esp_err_t ai_agent_save_memory_to_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(AI_AGENT_MEMORY_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_blob(handle, "memory", &s_memory, sizeof(s_memory));
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
        ESP_LOGI(TAG, "Memory saved to NVS");
    }

    nvs_close(handle);
    return ret;
}

static esp_err_t ai_agent_load_memory_from_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(AI_AGENT_MEMORY_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "No previous memory found");
        }
        return ret;
    }

    size_t size = sizeof(s_memory);
    ret = nvs_get_blob(handle, "memory", &s_memory, &size);
    nvs_close(handle);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Memory loaded: %u messages, summary: %.50s...", 
                 s_memory.message_count, s_memory.summary);
    }

    return ret;
}

/* ========== 历史管理函数 ========== */
static esp_err_t ai_agent_add_message(const char *role, const char *content)
{
    if (s_history_count >= AI_AGENT_MAX_SHORT_HISTORY) {
        ESP_LOGW(TAG, "History full, triggering summary");
        ai_agent_trigger_summary();
    }

    ai_agent_message_t *new_history = realloc(s_history, (s_history_count + 1) * sizeof(ai_agent_message_t));
    if (!new_history) {
        return ESP_ERR_NO_MEM;
    }

    s_history = new_history;
    s_history[s_history_count].role = strdup(role); 
    s_history[s_history_count].content = strdup(content);
    s_history[s_history_count].timestamp = (uint32_t)time(NULL);

    if (!s_history[s_history_count].role || !s_history[s_history_count].content) {
        free(s_history[s_history_count].role);
        free(s_history[s_history_count].content);
        return ESP_ERR_NO_MEM;
    }

    s_history_count++;
    return ESP_OK;
}

esp_err_t ai_agent_clear_history(void)
{
    for (size_t i = 0; i < s_history_count; i++) {
        free(s_history[i].role);
        free(s_history[i].content);
    }
    free(s_history);
    s_history = NULL;
    s_history_count = 0;
    
    ESP_LOGI(TAG, "History cleared");
    return ESP_OK;
}

esp_err_t ai_agent_clear_memory(void)
{
    memset(&s_memory, 0, sizeof(s_memory));
    ai_agent_clear_history();

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(AI_AGENT_MEMORY_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret == ESP_OK) {
        nvs_erase_key(handle, "memory");
        nvs_commit(handle);
        nvs_close(handle);
    }

    ESP_LOGI(TAG, "Memory cleared");
    return ESP_OK;
}

esp_err_t ai_agent_get_memory_summary(char *out_summary, size_t out_size)
{
    if (!out_summary || out_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    strlcpy(out_summary, s_memory.summary, out_size);
    return ESP_OK;
}

esp_err_t ai_agent_trigger_summary(void)
{
    ESP_LOGI(TAG, "Summary triggered with %d messages", s_history_count);
    // 简化版：直接保存历史计数，不调用AI总结
    s_memory.message_count += s_history_count;
    s_memory.last_update_time = (uint32_t)time(NULL);
    
    ai_agent_save_memory_to_nvs();
    ai_agent_clear_history();
    
    return ESP_OK;
}

#endif /* AI_AGENT_ENABLE_MEMORY */

static void ai_agent_dispatch_event(ai_agent_event_t event, const char *data)
{
    if (s_config.event_handler) {
        s_config.event_handler(event, data, s_config.ctx);
    }
}
// 清理全局 client（程序退出时调用）
static void cleanup_http_client(void) 
{
    if (g_ai_agent_http_config) {
        esp_http_client_cleanup(g_ai_agent_http_config);
        g_ai_agent_http_config = NULL;
    }
}

static esp_err_t ai_agent_http_event_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id != HTTP_EVENT_ON_DATA || !evt->user_data || !evt->data || evt->data_len <= 0) {
        return ESP_OK;
    }

    ai_agent_http_response_t *response = (ai_agent_http_response_t *)evt->user_data;
    size_t copy_len = (size_t)evt->data_len;
    if (response->data_len + copy_len >= response->buffer_size) {
        copy_len = response->buffer_size - response->data_len - 1;
        response->overflow = true;
        ESP_LOGW(TAG, "Response buffer overflow, data truncated");
        return ESP_OK; // 仍然返回 ESP_OK 以继续接收数据，但不再写入缓冲区
    }

    if (copy_len > 0) {
        memcpy(response->buffer + response->data_len, evt->data, copy_len);
        response->data_len += copy_len;
        response->buffer[response->data_len] = '\0';

    }

    return ESP_OK;
}

static char *ai_agent_build_request_body(const char *text)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *messages = cJSON_CreateArray();
    cJSON *system_message = cJSON_CreateObject();
    cJSON *user_message = cJSON_CreateObject();
    cJSON *thinking = cJSON_CreateObject();
    if (!root || !messages || !system_message || !user_message) {
        cJSON_Delete(root);
        cJSON_Delete(messages);
        cJSON_Delete(system_message);
        cJSON_Delete(user_message);
        return NULL;
    }

    cJSON_AddStringToObject(root, "model", AI_AGENT_MODEL);
    cJSON_AddBoolToObject(root, "stream", false);

    cJSON_AddStringToObject(system_message, "role", "system");
    cJSON_AddStringToObject(system_message, "content", AI_AGENT_SYSTEM_PROMPT);
    cJSON_AddItemToArray(messages, system_message);
    for(size_t i = 0; i < s_history_count; i++) {
        cJSON *history_message = cJSON_CreateObject();
        if (!history_message) {
            cJSON_Delete(root);
            return NULL;
        }
        cJSON_AddStringToObject(history_message, "role", s_history[i].role);
        cJSON_AddStringToObject(history_message, "content", s_history[i].content);
        cJSON_AddItemToArray(messages, history_message);
    }
    cJSON_AddStringToObject(user_message, "role", "user");
    cJSON_AddStringToObject(user_message, "content", text);
    cJSON_AddItemToArray(messages, user_message);

    cJSON_AddItemToObject(root, "messages", messages);

    cJSON_AddStringToObject(thinking, "type", "disabled");
    cJSON_AddItemToObject(root, "thinking", thinking);
    cJSON_AddBoolToObject(root, "enable_thinking", false);

    char *body = cJSON_PrintUnformatted(root);
    LOG_I(TAG, "AI agent request body: %s", body);
    cJSON_Delete(root);
    return body;
}

static esp_err_t ai_agent_parse_response_text(const char *json, char *out_text, size_t out_size)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    const cJSON *choices = cJSON_GetObjectItem(root, "choices");
    const cJSON *first_choice = cJSON_IsArray(choices) ? cJSON_GetArrayItem(choices, 0) : NULL;
    const cJSON *message = first_choice ? cJSON_GetObjectItem(first_choice, "message") : NULL;
    const cJSON *content = message ? cJSON_GetObjectItem(message, "content") : NULL;

    if (!cJSON_IsString(content) || !content->valuestring) {
        cJSON_Delete(root);
        return ESP_ERR_NOT_FOUND;
    }

    strlcpy(out_text, content->valuestring, out_size);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t ai_agent_init(const ai_agent_config_t *config)
{
#if AI_AGENT_ENABLE_MEMORY
    esp_err_t ret = ai_agent_load_memory_from_nvs();
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load memory: %s", esp_err_to_name(ret));
    }
#endif
    if (s_initialized) {
        return ESP_OK;
    }

    if (config) {
        s_config = *config;
    } else {
        memset(&s_config, 0, sizeof(s_config));
    }

    s_initialized = true;
    return ESP_OK;
}
// ESP_LOGI(TAG, "==== AI agent online chat test begin ====");

// if (!ai_agent_test_has_api_key()) {
//     ESP_LOGW(TAG, "Skip online chat test: AI_AGENT_API_KEY is not configured");
//     return ESP_ERR_INVALID_STATE;
// }

// ai_agent_test_ctx_t test_ctx = {0};
// ai_agent_config_t config = {
//     .text_handler = ai_agent_test_on_text,
//     .audio_handler = ai_agent_test_on_audio,
//     .event_handler = ai_agent_test_on_event,
//     .ctx = &test_ctx,
// };

// ESP_RETURN_ON_ERROR(ai_agent_init(&config), TAG, "ai_agent_init failed");
// esp_err_t ret = ai_agent_start();
// if (ret != ESP_OK) {
//     (void)ai_agent_deinit();
//     ESP_LOGE(TAG, "ai_agent_start failed: %s", esp_err_to_name(ret));
//     return ret;
// }
esp_err_t ai_agent_start(void)
{
    if (!s_initialized) {
        ESP_LOGI(TAG, "==== AI agent online chat begin ====");
        ai_agent_config_t config = {
            .text_handler = NULL,
            .audio_handler = NULL,
            .event_handler = NULL,
            .ctx = NULL,
        };
        ESP_RETURN_ON_ERROR(ai_agent_init(&config), TAG, "init ai_agent failed");
    }

    s_started = true;
    ai_agent_dispatch_event(AI_AGENT_EVENT_CONNECTED, NULL);
    return ESP_OK;
}

esp_err_t ai_agent_stop(void)
{
    if (!s_started) {
        return ESP_OK;
    }

    s_started = false;
    ai_agent_dispatch_event(AI_AGENT_EVENT_DISCONNECTED, NULL);
    return ESP_OK;
}

esp_err_t ai_agent_deinit(void)
{
    ESP_RETURN_ON_ERROR(ai_agent_stop(), TAG, "stop ai_agent failed");
    s_initialized = false;
    memset(&s_config, 0, sizeof(s_config));
    return ESP_OK;
}

esp_err_t ai_agent_send_text(const char *text)
{
    esp_err_t ret;
    if (!text || text[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
#if AI_AGENT_ENABLE_MEMORY
    ret = ai_agent_add_message("user", text);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to add user message: %s", esp_err_to_name(ret));
    }
#endif
    if (!s_started) {
        return ESP_ERR_INVALID_STATE;
    }

    char *request_body = ai_agent_build_request_body(text);
    if (!request_body) {
        return ESP_ERR_NO_MEM;
    }

    char *response_buffer = calloc(1, AI_AGENT_HTTP_RESPONSE_MAX_LEN);
    if (!response_buffer) {
        free(request_body);
        return ESP_ERR_NO_MEM;
    }

    ai_agent_http_response_t response = {
        .buffer = response_buffer,
        .buffer_size = AI_AGENT_HTTP_RESPONSE_MAX_LEN,
    };
    if (g_ai_agent_http_config == NULL) {
        esp_http_client_config_t http_config = {
            .url = AI_AGENT_API_URL,
            .method = HTTP_METHOD_POST,
            .timeout_ms = AI_AGENT_HTTP_TIMEOUT_MS,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .event_handler = ai_agent_http_event_handler,
            .user_data = &response,
            .keep_alive_idle = HTTP_KEEP_ALIVE_TIME,        // 空闲5秒后关闭连接
            .keep_alive_interval = 2,    // 2秒探测间隔
            .keep_alive_count = 3,       // 最多3次探测
            .disable_auto_redirect = true,  // 避免重定向导致状态混乱
            .buffer_size = 4096,         // 缓冲区大小
        };
        g_ai_agent_http_config = esp_http_client_init(&http_config);
        if (!g_ai_agent_http_config) {
            free(response_buffer);
            free(request_body);
            return ESP_ERR_NO_MEM;
        }
    }

    esp_http_client_set_header(g_ai_agent_http_config, "Content-Type", "application/json");
    esp_http_client_set_header(g_ai_agent_http_config, "Authorization", AI_AGENT_API_KEY);
    esp_http_client_set_post_field(g_ai_agent_http_config, request_body, strlen(request_body));

    ret = esp_http_client_perform(g_ai_agent_http_config);
    int status_code = esp_http_client_get_status_code(g_ai_agent_http_config);

    free(request_body);
    cleanup_http_client();

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "DeepSeek request failed: %s", esp_err_to_name(ret));
        ai_agent_dispatch_event(AI_AGENT_EVENT_ERROR, "http request failed");
        free(response_buffer);
        return ret;
    }

    if (status_code < 200 || status_code >= 300) {
        ESP_LOGE(TAG, "DeepSeek HTTP status: %d, response: %s", status_code, response_buffer);
        ai_agent_dispatch_event(AI_AGENT_EVENT_ERROR, response_buffer);
        free(response_buffer);
        return ESP_FAIL;
    }

    if (response.overflow) {
        ESP_LOGW(TAG, "DeepSeek response truncated, increase AI_AGENT_HTTP_RESPONSE_MAX_LEN");
    }

    ai_agent_dispatch_event(AI_AGENT_EVENT_MESSAGE, response_buffer);

    char *answer = calloc(1, AI_AGENT_RESPONSE_TEXT_MAX_LEN);
    if (!answer) {
        free(response_buffer);
        return ESP_ERR_NO_MEM;
    }

    ret = ai_agent_parse_response_text(response_buffer, answer, AI_AGENT_RESPONSE_TEXT_MAX_LEN);
    free(response_buffer);
    ESP_LOGI(TAG, "AI agent answer: %s", answer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Parse DeepSeek response failed: %s", esp_err_to_name(ret));
        ai_agent_dispatch_event(AI_AGENT_EVENT_ERROR, "parse response failed");
        free(answer);
        return ret;
    }
#if AI_AGENT_ENABLE_MEMORY
    ret = ai_agent_add_message("assistant", answer);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to add assistant message: %s", esp_err_to_name(ret));
    }

    if (s_history_count >= AI_AGENT_SUMMARY_INTERVAL) {
        ESP_LOGI(TAG, "History reached %d messages, triggering summary", s_history_count);
        ai_agent_trigger_summary();
    }
#endif
    if (s_config.text_handler) {
        s_config.text_handler(answer, s_config.ctx);
    }

    free(answer);
    return ESP_OK;
}

void ai_agent_send_audio(uint8_t *data, size_t len)
{
    (void)data;
    (void)len;
    ESP_LOGW(TAG, "Audio sending is not supported by DeepSeek chat/completions endpoint");
}

#if !AI_AGENT_ENABLE_MEMORY
esp_err_t ai_agent_clear_history(void) { return ESP_OK; }
esp_err_t ai_agent_clear_memory(void) { return ESP_OK; }
esp_err_t ai_agent_get_memory_summary(char *out_summary, size_t out_size) { 
    (void)out_summary; (void)out_size; 
    return ESP_ERR_NOT_SUPPORTED; 
}
esp_err_t ai_agent_trigger_summary(void) { return ESP_ERR_NOT_SUPPORTED; }
#endif