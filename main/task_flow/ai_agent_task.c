#include "ai_agent_task.h"

#include <stdlib.h>
#include <string.h>
#include "ai_agent.h"
#include "cJSON.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "logger.h"
#include "workflow.h"

#define TAG "AI_TASK"

bool ai_agent_try_extract_text_from_json(const void *ptr, size_t len, char *out_text, size_t out_size)
{
    if (!ptr || len == 0 || !out_text || out_size == 0) {
        return false;
    }

    char *json_text = calloc(1, len + 1);
    if (!json_text) {
        return false;
    }

    memcpy(json_text, ptr, len);

    cJSON *root = cJSON_Parse(json_text);
    free(json_text);

    if (!root) {
        return false;
    }

    const cJSON *text = cJSON_GetObjectItem(root, "text");
    if (!cJSON_IsString(text) || !text->valuestring) {
        cJSON_Delete(root);
        return false;
    }

    strlcpy(out_text, text->valuestring, out_size);
    cJSON_Delete(root);
    return out_size == 0 ? false : true;
}

esp_err_t ai_agent_task(struct wf_runtime *rt, uint8_t self_task_id, void *arg)
{
    (void)arg;

    wf_message_t msg = {0};
    TickType_t io_timeout_ticks = portMAX_DELAY;
    esp_err_t ret = wf_recv(rt, self_task_id, &msg, io_timeout_ticks);
    if (ret != ESP_OK) {
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Received message for AI agent task, len=%u", (unsigned)msg.value);
    char text[512] = {0};
    if (ai_agent_try_extract_text_from_json(msg.ptr, (size_t)msg.value, text, sizeof(text))) {
        LOG_I(TAG, "ai json text: %s", text);
        ai_agent_start();
        ret = ai_agent_send_text(text);
        vTaskDelay(pdMS_TO_TICKS(1000)); // 避免过快调用 ai_agent_stop 导致消息未完全处理
        if (ret != ESP_OK) {
            LOG_E(TAG, "ai_agent_send_text failed: %s", esp_err_to_name(ret));
            return ret;
        }
    } else {
        LOG_W(TAG, "Failed to extract text from AI agent message");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}
