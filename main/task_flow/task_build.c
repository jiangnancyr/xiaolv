#include "ai_agent_task.h"
#include "audio_workflow.h"
#include "esp_check.h"
#include "freertos/idf_additions.h"
#include "hal/i2s_types.h"
#include "logger.h"
#include "siliconflow_asr.h"
#include "siliconflow_tts.h"
#include "task_flow_config.h"
#include "task_manager.h"
#include <example_config.h>

static const char *TAG = "TASK_BUILD";

static task_manager_t g_task_manager;
static audio_wf_ctx_t ctx = {0};

esp_err_t start_chat_flow(void)
{
    ESP_RETURN_ON_ERROR(task_manager_init(&g_task_manager, "ai_chat_flow"), TAG, "task manager init failed");


    es8311_audio_cfg_t cfg = {
        .sample_rate = 16000,
        .bits_per_sample = I2S_DATA_BIT_WIDTH_16BIT,
        .channels = 1,
        .out_vol = 60,
        .in_gain_db = 37,
    };
    ESP_RETURN_ON_ERROR(audio_wf_ctx_init(&ctx, &cfg, 1600 * 2, pdMS_TO_TICKS(500)), TAG, "audio_wf_ctx_init failed");
    vTaskDelay(pdMS_TO_TICKS(1000));
    // 音频工作流任务
    ESP_RETURN_ON_ERROR(task_manager_add_task(&g_task_manager, AUDIO_WF_TASK_CAPTURE, &(tm_task_config_t){
        .name = "audio_wf_task_capture",
        .fn = audio_wf_task_capture,
        .arg = &ctx,
        .stack_size = 10240,
        .priority = 5,
        .core_id = -1,
    }), TAG, "add sensor task failed");
    // 音频播放任务
    ESP_RETURN_ON_ERROR(task_manager_add_task(&g_task_manager, AUDIO_WF_TASK_PLAYBACK, &(tm_task_config_t){
        .name = "audio_wf_task_playback",
        .fn = audio_wf_task_playback,
        .arg = &ctx,
        .stack_size = 10240,
        .priority = 5,
        .core_id = -1,
    }), TAG, "add filter task failed");
    // AI代理任务
    ESP_RETURN_ON_ERROR(task_manager_add_task(&g_task_manager, AI_AGENT_TASK, &(tm_task_config_t){
        .name = "ai_agent_task",
        .fn = ai_agent_task,
        .arg = NULL,
        .stack_size = 10240,
        .priority = 5,
        .core_id = -1,
    }), TAG, "add ai_agent_task failed");
    // ASR上传任务
    ESP_RETURN_ON_ERROR(task_manager_add_task(&g_task_manager, AUDIO_ASR_TASK_STREAM, &(tm_task_config_t){
        .name = "audio_stream_via_http_task",
        .fn = audio_stream_via_http_task,
        .arg = NULL,
        .stack_size = 10240,
        .priority = 5,
        .core_id = -1,
    }), TAG, "add upload task failed");

    siliconflow_tts_config_t tts_config = {
        .api_key = AI_TTS_API_KEY,
        .text = NULL,
        .voice = AI_TTS_VOICE,
        .model = AI_TTS_MODEL,
        .audio_buffer = NULL,
        .audio_size = 0,
    };
    // TTS任务
    ESP_RETURN_ON_ERROR(task_manager_add_task(&g_task_manager, AUDIO_TTS_TASK, &(tm_task_config_t){
        .name = "siliconflow_tts_task",
        .fn = siliconflow_tts_task,
        .arg = &tts_config,
        .stack_size = 8192,
        .priority = 5,
        .core_id = -1,
    }), TAG, "add tts task failed");

    const uint8_t voide_handle_tasks[] = {AUDIO_WF_TASK_CAPTURE, AUDIO_ASR_TASK_STREAM};
    ESP_RETURN_ON_ERROR(task_manager_add_step(&g_task_manager,
                                              "collect+filter",
                                              TM_STEP_PARALLEL,
                                              voide_handle_tasks,
                                              sizeof(voide_handle_tasks) / sizeof(voide_handle_tasks[0])),
                                              TAG, "add collect step failed");

    const uint8_t ai_rep_tasks[] = {AI_AGENT_TASK, AUDIO_TTS_TASK, AUDIO_WF_TASK_PLAYBACK};
    ESP_RETURN_ON_ERROR(task_manager_add_step(&g_task_manager,
                                              "answer+playback",
                                              TM_STEP_SERIAL,
                                              ai_rep_tasks,
                                              sizeof(ai_rep_tasks) / sizeof(ai_rep_tasks[0])),
                                              TAG, "add answer+playback step failed");

    ESP_RETURN_ON_ERROR(task_manager_start(&g_task_manager), TAG, "task manager start failed");
    while(1) {
        esp_err_t ret = task_manager_run_once(&g_task_manager);
        if (ret != ESP_OK) {
            LOG_E(TAG, "task_manager_run_once failed: %s", esp_err_to_name(ret));
        } else {
            LOG_I(TAG, "task_manager_run_once success");
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    return ESP_FAIL;
}

void chat_flow_manager_task(void *pvParameters)
{
    esp_err_t ret = start_chat_flow();
    if (ret != ESP_OK) {
        LOG_E(TAG, "start_chat_flow failed: %s", esp_err_to_name(ret));
    }
    vTaskDelete(NULL);
}