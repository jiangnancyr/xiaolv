#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AI_AGENT_EVENT_CONNECTED = 1,
    AI_AGENT_EVENT_DISCONNECTED,
    AI_AGENT_EVENT_MESSAGE,
    AI_AGENT_EVENT_ERROR,
} ai_agent_event_t;

typedef void (*ai_agent_text_handler_t)(const char *text, void *ctx);
typedef void (*ai_agent_audio_handler_t)(uint8_t *data, size_t len, void *ctx);
typedef void (*ai_agent_event_handler_t)(ai_agent_event_t event, const char *data, void *ctx);

typedef struct {
    ai_agent_text_handler_t text_handler;
    ai_agent_audio_handler_t audio_handler;
    ai_agent_event_handler_t event_handler;
    void *ctx;
} ai_agent_config_t;

esp_err_t ai_agent_init(const ai_agent_config_t *config);
esp_err_t ai_agent_start(void);
esp_err_t ai_agent_stop(void);
esp_err_t ai_agent_deinit(void);
esp_err_t ai_agent_send_text(const char *text);
void ai_agent_send_audio(uint8_t *data, size_t len);

esp_err_t ai_agent_clear_history(void);
esp_err_t ai_agent_clear_memory(void);
esp_err_t ai_agent_get_memory_summary(char *out_summary, size_t out_size);
esp_err_t ai_agent_trigger_summary(void);

#ifdef __cplusplus
}
#endif
