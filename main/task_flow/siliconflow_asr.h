#ifndef SILICONFLOW_ASR_H
#define SILICONFLOW_ASR_H

#include "task_flow_config.h"
#include "workflow.h"
#include "esp_err.h"
#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *model_path;
    const char *lang;
    const char *voice;
    const char *speed;
    const char *pitch;
    const char *volume;
} siliconflow_asr_cfg_t;

esp_err_t audio_stream_via_http_task(struct wf_runtime *rt, uint8_t self_task_id, void *arg);



#ifdef __cplusplus
}
#endif

#endif /* SILICONFLOW_ASR_H */