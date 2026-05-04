// 示例：如何使用SiliconFlow TTS任务
#include "siliconflow_tts.h"
#include "task_flow_config.h"

// 在某个地方调用TTS
void example_tts_usage() {
    // TTS配置
    uint8_t *audio_data = NULL;
    size_t audio_size = 0;

    siliconflow_tts_config_t tts_config = {
        .api_key = "your_api_key_here",  // 替换为实际API密钥
        .text = "你站在桥上看风景，看风景的人在楼上看你。明月装饰了你的窗子，你装饰了别人的梦",
        .voice = "fnlp/MOSS-TTSD-v0.5:alex",
        .model = "fnlp/MOSS-TTSD-v0.5",
        .audio_buffer = audio_data,
        .audio_size = audio_size,
    };

    // 发送TTS任务消息
    // 假设有workflow运行时rt
    // wf_send(rt, current_task_id, AUDIO_TTS_TASK, SOME_MSG_ID, sizeof(tts_config), &tts_config, timeout);

    // 使用完成后释放内存
    if (audio_data) {
        // 处理音频数据，例如播放或保存
        // ...

        // 释放内存
        free(audio_data);
        audio_data = NULL;
    }
}