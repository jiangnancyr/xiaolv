#ifndef AUDIO_WORKFLOW_H
#define AUDIO_WORKFLOW_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "workflow.h"
#include "es8311_audio.h"
#include "task_flow_config.h"
#ifdef __cplusplus
extern "C" {
#endif
esp_err_t audio_wf_ctx_init(audio_wf_ctx_t *ctx,
                            const es8311_audio_cfg_t *audio_cfg,
                            size_t pcm_buf_size,
                            TickType_t io_timeout_ticks);
esp_err_t audio_wf_ctx_deinit(audio_wf_ctx_t *ctx);

esp_err_t audio_wf_task_capture(struct wf_runtime *rt, uint8_t self_task_id, void *arg);
esp_err_t audio_wf_task_playback(struct wf_runtime *rt, uint8_t self_task_id, void *arg);

const wf_def_t *audio_wf_get_loopback_def(audio_wf_ctx_t *ctx);

#ifdef __cplusplus
}
#endif


#endif /* AUDIO_WORKFLOW_H */
