#include "ai_agent_tests.h"

#include <string.h>
#include "ai_agent.h"
#include "esp_check.h"
#include "esp_log.h"
#include "example_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#define TAG "AI_AGENT_TEST"
#define AI_AGENT_TEST_REPLY_WAIT_MS 10000

typedef struct {
    int text_count;
    int event_count;
    int audio_count;
} ai_agent_test_ctx_t;

static void ai_agent_test_on_text(const char *text, void *ctx)
{
    ai_agent_test_ctx_t *test_ctx = (ai_agent_test_ctx_t *)ctx;
    if (test_ctx) {
        test_ctx->text_count++;
    }
    ESP_LOGI(TAG, "AI text: %s", text ? text : "");
}

static void ai_agent_test_on_audio(uint8_t *data, size_t len, void *ctx)
{
    ai_agent_test_ctx_t *test_ctx = (ai_agent_test_ctx_t *)ctx;
    if (test_ctx) {
        test_ctx->audio_count++;
    }
    ESP_LOGI(TAG, "AI audio data: %u bytes", (unsigned)len);
    (void)data;
}

static void ai_agent_test_on_event(ai_agent_event_t event, const char *data, void *ctx)
{
    ai_agent_test_ctx_t *test_ctx = (ai_agent_test_ctx_t *)ctx;
    if (test_ctx) {
        test_ctx->event_count++;
    }
    ESP_LOGI(TAG, "AI event: %d, data: %s", event, data ? data : "");
}

static bool ai_agent_test_has_api_key(void)
{
    return strcmp(AI_AGENT_API_KEY, "Bearer YOUR_DEEPSEEK_API_KEY") != 0 &&
           strlen(AI_AGENT_API_KEY) > strlen("Bearer ");
}

esp_err_t ai_agent_test_init_deinit(void)
{
    ESP_LOGI(TAG, "==== AI agent init/deinit test begin ====");

    ai_agent_test_ctx_t test_ctx = {0};
    ai_agent_config_t config = {
        .text_handler = ai_agent_test_on_text,
        .audio_handler = ai_agent_test_on_audio,
        .event_handler = ai_agent_test_on_event,
        .ctx = &test_ctx,
    };

    ESP_RETURN_ON_ERROR(ai_agent_init(&config), TAG, "ai_agent_init failed");
    ESP_RETURN_ON_ERROR(ai_agent_deinit(), TAG, "ai_agent_deinit failed");

    ESP_LOGI(TAG, "==== AI agent init/deinit test passed ====");
    return ESP_OK;
}

esp_err_t ai_agent_test_online_chat(void)
{
    ESP_LOGI(TAG, "==== AI agent online chat test begin ====");

    if (!ai_agent_test_has_api_key()) {
        ESP_LOGW(TAG, "Skip online chat test: AI_AGENT_API_KEY is not configured");
        return ESP_ERR_INVALID_STATE;
    }

    ai_agent_test_ctx_t test_ctx = {0};
    ai_agent_config_t config = {
        .text_handler = ai_agent_test_on_text,
        .audio_handler = ai_agent_test_on_audio,
        .event_handler = ai_agent_test_on_event,
        .ctx = &test_ctx,
    };

    ESP_RETURN_ON_ERROR(ai_agent_init(&config), TAG, "ai_agent_init failed");
    esp_err_t ret = ai_agent_start();
    if (ret != ESP_OK) {
        (void)ai_agent_deinit();
        ESP_LOGE(TAG, "ai_agent_start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = ai_agent_send_text("请用一句话介绍你自己。最多二十个字。");
    if (ret != ESP_OK) {
        (void)ai_agent_deinit();
        ESP_LOGE(TAG, "ai_agent_send_text failed: %s", esp_err_to_name(ret));
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(AI_AGENT_TEST_REPLY_WAIT_MS));

    ESP_LOGI(TAG, "AI agent test stats: text=%d event=%d audio=%d",
             test_ctx.text_count, test_ctx.event_count, test_ctx.audio_count);

    ESP_RETURN_ON_ERROR(ai_agent_deinit(), TAG, "ai_agent_deinit failed");

    ESP_LOGI(TAG, "==== AI agent online chat test end ====");
    return ESP_OK;
}

void ai_agent_test_run_all(void *pvParameters)
{
    esp_err_t ret = ai_agent_test_init_deinit();
    ESP_LOGI(TAG, "AI agent init/deinit test: %s", esp_err_to_name(ret));

#if defined(CONFIG_AI_AGENT_TEST_ONLINE_CHAT) && CONFIG_AI_AGENT_TEST_ONLINE_CHAT
    ret = ai_agent_test_online_chat();
    ESP_LOGI(TAG, "AI agent online chat test: %s", esp_err_to_name(ret));
#endif
    vTaskDelete(NULL);
}
