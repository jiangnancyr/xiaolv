#include "ai_agent_task.h"

#include <stdlib.h>
#include <string.h>
#include "ai_agent.h"
#include "cJSON.h"
#include "esp_err.h"
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
    return true;
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

    char text[512] = {0};
    if (ai_agent_try_extract_text_from_json(msg.ptr, (size_t)msg.value, text, sizeof(text))) {
        LOG_I(TAG, "ai json text: %s", text);
        ai_agent_start();
        return ai_agent_send_text(text);
    }

    LOG_I(TAG, "ai rev:%s, len=%d", (const char *)msg.ptr, msg.value);
    ai_agent_start();
    return ai_agent_send_text((const char *)msg.ptr);
}
