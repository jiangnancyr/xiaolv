#include "siliconflow_asr.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include "esp_check.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_netif_types.h"
#include "example_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
// WebSocket方式发送实时音频流
#include "pcm_to_wav.h"


static const char *TAG = "AUDIO_ASR";
esp_http_client_handle_t g_http_asr_client = NULL;
static bool s_connected = false;
static int s_idle_seconds = 0;           // 空闲秒数
static struct  wf_runtime *asr_rt = NULL;
static uint8_t asr_task_id = 0;
// 可选：读取响应
// char response_buffer[1024];
// API配置
#define API_URL "https://api.siliconflow.cn/v1/audio/transcriptions"
#define BOUNDARY "--------------------------ESP32StreamBoundary"
#define CHUNK_SIZE 1024  // 每次发送的音频块大小
#define PART_HEADER   "--" BOUNDARY "\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\nFunAudioLLM/SenseVoiceSmall"
#define FILE_HEADER   "--" BOUNDARY "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"chat.wav\"\r\n\r\n"
#define FOOTER        "\r\n--" BOUNDARY "--\r\n"


typedef struct {
    char *data;      // 音频数据指针
    size_t size;        // 当前块大小
    bool is_last;       // 是否最后一块
} audio_chunk_t;

char *rep_data = NULL;
// 1. 定义事件处理函数
static esp_err_t siliconflow_asr_event_handler(esp_http_client_event_t *data)
{
    switch (data->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
            s_idle_seconds = 0;  // 收到数据也算活动
            s_connected = true;
            rep_data = malloc(512);
            if (!rep_data) {        
                ESP_LOGE(TAG, "Failed to allocate response buffer");
                return ESP_ERR_NO_MEM;
            }
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_DISCONNECTED:
            s_connected = false;
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_ON_DATA: // 在 case 分支中处理接收到的数据
            s_idle_seconds = 0;
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA");
            // 注意: data->data_ptr 不保证以 '\0' 结尾，不能直接用 %s 打印。
            // 标准做法是使用 data->data_len 精确控制打印长度。
            memcpy(rep_data, data->data, data->data_len);
            size_t index = data->data_len < 512 ? data->data_len : 511;
            rep_data[index] = '\0';
            if (data->data_len > 0) {
                ESP_LOGI(TAG, "Received opcode=%s, len=%d", rep_data, data->data_len);
                esp_err_t er = wf_send(asr_rt, asr_task_id, AI_AGENT_TASK, USER_CHAT_COINTEXT, (uint32_t)index, rep_data, 0);
                ESP_LOGI(TAG, "wf_send result: %d", er);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            s_idle_seconds = 0;
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        default:
            break;
    }
    return ESP_OK;
}

static esp_err_t audio_build_build_context(char *audio_buffer, size_t audio_len, char **post_data, size_t *len)
{
        // 1. 计算整个请求体的大小
        // 文本字段部分
        const char *text_part_header = PART_HEADER;
        const char *text_part_footer = "\r\n";
        // 文件头部分
        char *file_header = FILE_HEADER;
        // 结束边界
        const char *footer = FOOTER;
        wav_header_t wav_header;
        generate_wav_header(&wav_header, 16000, 1, 16, audio_len);
        // 计算总长度
        size_t total_len = strlen(text_part_header) + strlen(text_part_footer) +
                           strlen(file_header) + audio_len + sizeof(wav_header_t) + strlen(footer);
        
        ESP_LOGI(TAG, "Total request body size: %zu bytes", total_len);

        // 2. 分配缓冲区并构建完整请求体
        *post_data = heap_caps_malloc(total_len, MALLOC_CAP_SPIRAM);
        if (!post_data) {
            ESP_LOGE(TAG, "Failed to allocate post buffer");
            return ESP_FAIL;
        }
        
        char *ptr = *post_data;
        // 复制文本字段
        memcpy(ptr, text_part_header, strlen(text_part_header));
        ptr += strlen(text_part_header);
        memcpy(ptr, text_part_footer, strlen(text_part_footer));
        ptr += strlen(text_part_footer);
        // 复制文件头
        memcpy(ptr, file_header, strlen(file_header));
        ptr += strlen(file_header);
        // 转成wav格式
        memcpy(ptr, &wav_header, sizeof(wav_header));
        ptr += sizeof(wav_header);
        // 复制音频数据
        memcpy(ptr, audio_buffer, audio_len);
        ptr += audio_len;
        *len = total_len + 1;
        // 复制结束边界
        memcpy(ptr, footer, strlen(footer));
        return ESP_OK;
}

// 清理全局 client（程序退出时调用）
static void cleanup_http_client(void) 
{
    if (g_http_asr_client) {
        esp_http_client_cleanup(g_http_asr_client);
        g_http_asr_client = NULL;
    }
}

esp_err_t audio_stream_via_http_task(struct wf_runtime *rt, uint8_t self_task_id, void *arg)
{   
    asr_rt = rt;
    asr_task_id = self_task_id;
    esp_err_t ret = ESP_OK;
    // 初始化websocket客户端
    if (g_http_asr_client == NULL) {
        esp_http_client_config_t config = {
            .url = API_URL,
            .method = HTTP_METHOD_POST,
            .timeout_ms = 5000,  // 增加超时时间到30秒
            .keep_alive_enable = true,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .event_handler = siliconflow_asr_event_handler,
            .keep_alive_idle = HTTP_KEEP_ALIVE_TIME,        // 空闲5秒后关闭连接
            .keep_alive_interval = 2,    // 2秒探测间隔
            .keep_alive_count = 3,       // 最多3次探测
            .disable_auto_redirect = true,  // 避免重定向导致状态混乱
            .buffer_size = 4096,         // 缓冲区大小
        };
        
        g_http_asr_client = esp_http_client_init(&config);
    }
    // 持续发送音频块
    wf_message_t msg = {0};
    TickType_t io_timeout_ticks = portMAX_DELAY;
    char *post_data = NULL;
    size_t len = 0;
    ret = wf_recv(rt, self_task_id, &msg, io_timeout_ticks);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "wf_recvs fail");
    ESP_LOGI(TAG, "msg.size=%d", msg.value);
    
    ret = audio_build_build_context(msg.ptr, msg.value, &post_data, &len);
    // printf("%s\r\n", post_data);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "build context fail");
    // 2. 重置 client 状态（关键安全步骤）
    ret = esp_http_client_set_method(g_http_asr_client, HTTP_METHOD_POST);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "esp_http_client_set_method fail");
    // 3. 清除旧的 POST 字段（重要！）
    // ret = esp_http_client_set_post_field(g_http_asr_client, NULL, 0);
    // ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "esp_http_client_set_post_field clear fail");
    // 3. 设置headers
    ret = esp_http_client_set_header(g_http_asr_client, "Authorization", SILICONFLOW_API_KEY);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "esp_http_client_set_header fail");
    ret = esp_http_client_set_header(g_http_asr_client, "User-Agent", "ESP32/1.0");
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "esp_http_client_set_header fail");
    // 4. 设置关键的Content-Type头
    char content_type[128];
    snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=%s", BOUNDARY);
    ret = esp_http_client_set_header(g_http_asr_client, "Content-Type", content_type);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "esp_http_client_set_header fail");
    // 设置写回调函数 (这是IDF实现流式上传的核心)
    ret = esp_http_client_set_post_field(g_http_asr_client, post_data, len);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "esp_http_client_set_post_field fail");

    // 添加重试机制，最多重试3次
    int retry_count = 0;
    const int max_retries = 5;
    do {
        ret = esp_http_client_perform(g_http_asr_client);
        if (ret == ESP_OK) {
            break;
        }
        retry_count++;
        if (retry_count < max_retries) {
            ESP_LOGW(TAG, "HTTP request failed (attempt %d/%d), retrying...", retry_count, max_retries);
            vTaskDelay(pdMS_TO_TICKS(1000));  // 等待1秒后重试
        }
    } while (retry_count < max_retries);
    
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "esp_http_client_perform fail after %d retries", max_retries);

    int status_code = esp_http_client_get_status_code(g_http_asr_client);
    ESP_LOGI(TAG, "HTTP POST Status = %d", status_code);
    // // 这里没判断读取结果是否超上限
    // int read_len = esp_http_client_read_response(g_http_asr_client, response_buffer, sizeof(response_buffer));
    // if (read_len > 0) {
    //     ESP_LOGI(TAG, "Response: %.*s", read_len, response_buffer);
    // }
    // 这里后续要改进，申请内存太多了。
    if (post_data != NULL) {
        free(post_data);
    }
    if (msg.ptr != NULL) {
        free(msg.ptr);
    }
    return ESP_OK;
err:
    // 这里后续要改进，申请内存太多了。
    if (post_data != NULL) {
        free(post_data);
    }
    if (msg.ptr != NULL) {
        free(msg.ptr);
    }
    cleanup_http_client();
    return ret;
}
