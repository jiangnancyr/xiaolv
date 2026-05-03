#include "audio_workflow.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "esp_heap_caps.h"
#include "esp_log.h"

#define TAG "AUDIO_WF"

static const uint8_t s_step_stream_tasks[] = {AUDIO_WF_TASK_CAPTURE, AUDIO_WF_TASK_PLAYBACK};
static const wf_step_desc_t s_steps[] = {
    {
        .type = WF_STEP_PARALLEL,
        .task_ids = s_step_stream_tasks,
        .task_count = 2,
        .name = "capture+playback",
    },
};

static bool audio_chunk_is_silent_16bit(const uint8_t *buf, size_t len, int threshold_abs)
{
    if (!buf || len < 2) {
        return true;
    }
    int32_t sum = 0;
    const int16_t *samples = (const int16_t *)buf;
    size_t sample_count = len / sizeof(int16_t);
    for (size_t i = 0; i < sample_count; i++) {
        int s = samples[i];
        if (s < 0) {
            s = -s;
        }
        sum += s;
    }
    // ESP_LOGI(TAG, "average voice=%d\r\n", sum/len);
    if (sum/len > threshold_abs) {
        return false;
    }
    return true;
}

esp_err_t audio_wf_ctx_init(audio_wf_ctx_t *ctx,
                            const es8311_audio_cfg_t *audio_cfg,
                            size_t pcm_buf_size,
                            TickType_t io_timeout_ticks)
{
    if (!ctx || !audio_cfg || pcm_buf_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->audio_cfg = *audio_cfg;
    ctx->pcm_buf_size = pcm_buf_size;
    ctx->io_timeout_ticks = io_timeout_ticks;
    ctx->silence_threshold_abs = 750;  // 降低静音阈值以减少误判
    ctx->silence_chunks_to_stop = 30;
    ctx->min_recording_chunks = 50;  // 最少录制50个块（约2-3秒，取决于块大小）
    ctx->capture_buf = (uint8_t *)malloc(pcm_buf_size);
    if (!ctx->capture_buf) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = es8311_audio_init(&ctx->audio_cfg);
    if (err != ESP_OK) {
        free(ctx->capture_buf);
        ctx->capture_buf = NULL;
        return err;
    }
    ctx->audio_inited = true;
    return ESP_OK;
}

esp_err_t audio_wf_ctx_deinit(audio_wf_ctx_t *ctx)
{
    if (!ctx) {
        return ESP_ERR_INVALID_ARG;
    }

    if (ctx->audio_inited) {
        (void)es8311_audio_deinit();
        ctx->audio_inited = false;
    }
    if (ctx->capture_buf) {
        free(ctx->capture_buf);
        ctx->capture_buf = NULL;
    }
    return ESP_OK;
}

esp_err_t audio_wf_task_capture(struct wf_runtime *rt, uint8_t self_task_id, void *arg)
{
    audio_wf_ctx_t *ctx = (audio_wf_ctx_t *)arg;
    if (!rt || !ctx || !ctx->audio_inited || !ctx->capture_buf) {
        return ESP_ERR_INVALID_STATE;
    }
    const size_t MAX_AUDIO_DATA = 640000; // 16000 * 2 * 20;
    int silent_count = 0;
    uint8_t *merged_buf = NULL;
    size_t merged_len = 0;
    // 获取 SPIRAM 剩余字节数
    size_t free_spiram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "Free SPIRAM: %zu bytes\n", free_spiram);
    merged_buf = (uint8_t *)heap_caps_malloc(MAX_AUDIO_DATA, MALLOC_CAP_SPIRAM);
    if(merged_buf == NULL) {
        ESP_LOGE(TAG, "malloc merged_buf error");
    }
    bool speech_started = false;
    bool has_speech = false;
    while (1) {
        size_t bytes_read = 0;
        esp_err_t err = es8311_audio_input(ctx->capture_buf, ctx->pcm_buf_size, &bytes_read, ctx->io_timeout_ticks);
        if (err != ESP_OK) {
            free(merged_buf);
            return err;
        }
        if (bytes_read == 0) {
            continue;
        }
        if (bytes_read > SIZE_MAX - merged_len || merged_len + bytes_read > UINT32_MAX) {
            free(merged_buf);
            return ESP_ERR_INVALID_SIZE;
        }
        // 是否讲话了
        has_speech = !audio_chunk_is_silent_16bit(ctx->capture_buf, bytes_read, ctx->silence_threshold_abs);
        if (!speech_started && has_speech) {
            silent_count = 0; 
            speech_started = true;
            ESP_LOGW(TAG, "speech start!!!");
        }
        if (speech_started) {
            // 如果累计的数据量大于上限，则重新分配内存。
            if (merged_len + bytes_read > MAX_AUDIO_DATA) {
                uint8_t *new_buf = (uint8_t *)realloc(merged_buf, merged_len + bytes_read);
                ESP_LOGW(TAG, "voice time > 20s");
                if (!new_buf) {
                    free(merged_buf);
                    return ESP_ERR_NO_MEM;
                }
                merged_buf = new_buf;
            }
            memcpy(merged_buf + merged_len, ctx->capture_buf, bytes_read);
            merged_len += bytes_read;
            if (!has_speech) {
                silent_count++;
                // 只有在录制了足够的块数后才因静音停止
                if (silent_count >= ctx->silence_chunks_to_stop && merged_len >= ctx->min_recording_chunks * bytes_read) {
                    speech_started = false;
                    silent_count = 0;
                    if (merged_len > 0) {
                        err = wf_send(rt, self_task_id, AUDIO_ASR_TASK_STREAM, AUDIO_WF_MSG_PCM,
                                      (uint32_t)merged_len, merged_buf, ctx->io_timeout_ticks);
                        if (err != ESP_OK) {
                            free(merged_buf);
                            return err;
                        }
                        merged_buf = NULL;
                        merged_len = 0;
                    }
        
                    // err = wf_send(rt, self_task_id, AUDIO_ASR_TASK_STREAM, AUDIO_WF_MSG_END, 0, NULL, ctx->io_timeout_ticks);
                    // if (err != ESP_OK) {
                    //     return err;
                    // }
                    ESP_LOGI(TAG, "capture stop by silence: chunks=%d", silent_count);
                    return ESP_OK;
                }
            } else {
                silent_count = 0;
            }
        }
    }
}

esp_err_t audio_wf_task_playback(struct wf_runtime *rt, uint8_t self_task_id, void *arg)
{
    audio_wf_ctx_t *ctx = (audio_wf_ctx_t *)arg;
    if (!rt || !ctx || !ctx->audio_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    while (1) {
        wf_message_t msg = {0};
        esp_err_t err = wf_recv(rt, self_task_id, &msg, ctx->io_timeout_ticks);
        if (err == ESP_ERR_TIMEOUT) {
            continue;
        }
        if (err != ESP_OK) {
            return err;
        }

        if (msg.msg_id == AUDIO_WF_MSG_END) {
            ESP_LOGI(TAG, "playback received end message");
            return ESP_OK;
        }
        if (msg.msg_id != AUDIO_WF_MSG_PCM || !msg.ptr || msg.value == 0) {
            continue;
        }

        size_t bytes_write = 0;
        err = es8311_audio_output(msg.ptr, msg.value, &bytes_write, ctx->io_timeout_ticks);
        free(msg.ptr);
        if (err != ESP_OK) {
            return err;
        }
    }
}

const wf_def_t *audio_wf_get_loopback_def(audio_wf_ctx_t *ctx)
{
    static wf_task_desc_t s_tasks[2];
    static wf_def_t s_def;

    if (!ctx) {
        return NULL;
    }

    s_tasks[0] = (wf_task_desc_t)WF_TASK(AUDIO_WF_TASK_CAPTURE, "audio_capture", audio_wf_task_capture, ctx, 4096, 5, -1);
    s_tasks[1] = (wf_task_desc_t)WF_TASK(AUDIO_WF_TASK_PLAYBACK, "audio_playback", audio_wf_task_playback, ctx, 4096, 5, -1);

    s_def.name = "audio_loopback_flow";
    s_def.tasks = s_tasks;
    s_def.task_count = 2;
    s_def.steps = s_steps;
    s_def.step_count = 1;
    return &s_def;
}
