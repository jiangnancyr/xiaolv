#ifndef SILICONFLOW_TTS_H
#define SILICONFLOW_TTS_H

#include "esp_err.h"
#include "task_flow_config.h"

#ifdef __cplusplus
extern "C" {
#endif
struct wf_runtime;  // 前向声明wf_runtime结构体，避免直接包含workflow.h
// TTS任务函数声明
esp_err_t siliconflow_tts_task(struct wf_runtime *rt, uint8_t self_task_id, void *arg);
// Note: Ensure this declaration matches the implementation in siliconflow_tts.c

// TTS配置结构体
typedef struct {
    const char *api_key;
    const char *text;
    const char *voice;
    const char *model;
    uint8_t *audio_buffer;  // 输出音频数据缓冲区
    size_t audio_size;      // 音频数据大小
} siliconflow_tts_config_t;

#ifdef __cplusplus
}
#endif

#endif // SILICONFLOW_TTS_H