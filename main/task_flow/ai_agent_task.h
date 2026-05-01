#ifndef AI_AGENT_TASK_H
#define AI_AGENT_TASK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

struct wf_runtime;

bool ai_agent_try_extract_text_from_json(const void *ptr, size_t len, char *out_text, size_t out_size);
esp_err_t ai_agent_task(struct wf_runtime *rt, uint8_t self_task_id, void *arg);

#ifdef __cplusplus
}
#endif

#endif
