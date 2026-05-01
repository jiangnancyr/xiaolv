#ifndef TASK_FLOW_CONFIG_H
#define TASK_FLOW_CONFIG_H

#include "sdkconfig.h"
#include "es8311_audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdbool.h"
#include "stdint.h"
#include "stddef.h"

#define TASK_FLOW_MAX_TASKS 10
#define TASK_FLOW_MAX_STEPS 10
#define TASK_FLOW_MAX_MESSAGES 10
#define TASK_FLOW_MAX_QUEUES 10
#define TASK_FLOW_MAX_EVENTS 10
#define TASK_FLOW_MAX_TIMERS 10
#define TASK_FLOW_MAX_SEMAPHORES 10
#define TASK_FLOW_MAX_MUTEXES 10
#define TASK_FLOW_MAX_CONDVARS 10

#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    AUDIO_WF_TASK_CAPTURE = 1,
    AUDIO_WF_TASK_PLAYBACK = 2,
    AUDIO_ASR_TASK_STREAM = 3,
    AI_AGENT_TASK = 4,
    AUDIO_WS_KEEP_TASK = 5,
} audio_wf_task_id_t;

typedef enum {
    AUDIO_WF_MSG_PCM = 1,
    AUDIO_WF_MSG_END = 2,
    AUDIO_ASR_MSG_STREAM = 3,
    USER_CHAT_COINTEXT = 4,
} audio_wf_msg_id_t;

typedef struct {
    es8311_audio_cfg_t audio_cfg;
    uint8_t *capture_buf;
    size_t pcm_buf_size;
    TickType_t io_timeout_ticks;
    int silence_threshold_abs;
    int silence_chunks_to_stop;
    bool audio_inited;
} audio_wf_ctx_t;

#ifdef __cplusplus
}
#endif

#endif /* TASK_FLOW_CONFIG_H */

