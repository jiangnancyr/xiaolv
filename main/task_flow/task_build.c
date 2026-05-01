#include "audio_workflow.h"
#include "esp_check.h"
#include "hal/i2s_types.h"
#include "siliconflow_asr.h"
#include "task_flow_config.h"
#include "task_manager.h"

static const char *TAG = "TASK_BUILD";

static task_manager_t g_task_manager;
static audio_wf_ctx_t ctx = {0};

esp_err_t start_chat_flow(void)
{
    ESP_RETURN_ON_ERROR(task_manager_init(&g_task_manager, "demo_flow"), TAG, "task manager init failed");


    es8311_audio_cfg_t cfg = {
        .sample_rate = 16000,
        .bits_per_sample = I2S_DATA_BIT_WIDTH_16BIT,
        .channels = 1,
        .out_vol = 60,
        .in_gain_db = 37,
    };
    ESP_RETURN_ON_ERROR(audio_wf_ctx_init(&ctx, &cfg, 1024, pdMS_TO_TICKS(500)), TAG, "audio_wf_ctx_init failed");
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    ESP_RETURN_ON_ERROR(task_manager_add_task(&g_task_manager, AUDIO_WF_TASK_CAPTURE, &(tm_task_config_t){
        .name = "audio_wf_task_capture",
        .fn = audio_wf_task_capture,
        .arg = &ctx,
        .stack_size = 10240,
        .priority = 5,
        .core_id = -1,
    }), TAG, "add sensor task failed");

    ESP_RETURN_ON_ERROR(task_manager_add_task(&g_task_manager, AUDIO_WF_TASK_PLAYBACK, &(tm_task_config_t){
        .name = "audio_wf_task_playback",
        .fn = audio_wf_task_playback,
        .arg = &ctx,
        .stack_size = 10240,
        .priority = 5,
        .core_id = -1,
    }), TAG, "add filter task failed");

    ESP_RETURN_ON_ERROR(task_manager_add_task(&g_task_manager, AUDIO_ASR_TASK_STREAM, &(tm_task_config_t){
        .name = "audio_stream_via_http_task",
        .fn = audio_stream_via_http_task,
        .arg = NULL,
        .stack_size = 10240,
        .priority = 5,
        .core_id = -1,
    }), TAG, "add upload task failed");

    const uint8_t voide_handle_tasks[] = {AUDIO_WF_TASK_CAPTURE, AUDIO_ASR_TASK_STREAM};
    ESP_RETURN_ON_ERROR(task_manager_add_step(&g_task_manager,
                                              "collect+filter",
                                              TM_STEP_PARALLEL,
                                              voide_handle_tasks,
                                              sizeof(voide_handle_tasks) / sizeof(voide_handle_tasks[0])),
                                              TAG, "add collect step failed");

    const uint8_t ai_rep_tasks[] = { AUDIO_WF_TASK_PLAYBACK};
    ESP_RETURN_ON_ERROR(task_manager_add_step(&g_task_manager,
                                              "upload",
                                              TM_STEP_SERIAL,
                                              ai_rep_tasks,
                                              sizeof(ai_rep_tasks) / sizeof(ai_rep_tasks[0])),
                                              TAG, "add upload step failed");

    ESP_RETURN_ON_ERROR(task_manager_start(&g_task_manager), TAG, "task manager start failed");
    return task_manager_run_once(&g_task_manager);
}