/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "sdkconfig.h"

/* Example configurations */
#define EXAMPLE_RECV_BUF_SIZE   (2400)
#define EXAMPLE_SAMPLE_RATE     (16000)
#define EXAMPLE_MCLK_MULTIPLE   (384) // If not using 24-bit data width, 256 should be enough
#define EXAMPLE_MCLK_FREQ_HZ    (EXAMPLE_SAMPLE_RATE * EXAMPLE_MCLK_MULTIPLE)
#define EXAMPLE_VOICE_VOLUME    CONFIG_EXAMPLE_VOICE_VOLUME
#define EXAMPLE_PA_CTRL_IO      CONFIG_EXAMPLE_PA_CTRL_IO
#if CONFIG_EXAMPLE_MODE_ECHO
#define EXAMPLE_MIC_GAIN        CONFIG_EXAMPLE_MIC_GAIN
#endif

/* I2C port and GPIOs */
#define I2C_NUM         (0)
#define I2C_SCL_IO      CONFIG_EXAMPLE_I2C_SCL_IO
#define I2C_SDA_IO      CONFIG_EXAMPLE_I2C_SDA_IO

/* I2S port and GPIOs */
#define I2S_NUM         (0)
#define I2S_MCK_IO      CONFIG_EXAMPLE_I2S_MCLK_IO
#define I2S_BCK_IO      CONFIG_EXAMPLE_I2S_BCLK_IO
#define I2S_WS_IO       CONFIG_EXAMPLE_I2S_WS_IO
#define I2S_DO_IO       CONFIG_EXAMPLE_I2S_DOUT_IO
#define I2S_DI_IO       CONFIG_EXAMPLE_I2S_DIN_IO

// ==================== 系统配置 ====================
#define SYSTEM_VERSION          "1.0.0"
#define SYSTEM_NAME             "ESP32-S3 Voice Assistant"

// 任务优先级配置
#define TASK_PRIORITY_HIGH      5
#define TASK_PRIORITY_MID       3
#define TASK_PRIORITY_LOW       1

// 任务栈大小 (字节)
#define TASK_STACK_MAIN         8192
#define TASK_STACK_LED          4096
#define TASK_STACK_AI           20*1024
#define TASK_STACK_AUDIO        8192
#define TASK_STACK_DISPLAY      4096
#define TASK_STACK_NETWORK      8192
#define TASK_STACK_SKILL        4096
#define TASK_STACK_DISPATCHER   20480
// 事件队列大小
#define EVENT_QUEUE_SIZE        32

// ==================== WiFi 配置 ====================
#define WIFI_SSID               "CU_d2P3"
#define WIFI_PASSWORD           "kd4gh9rx"
#define WIFI_MAX_RETRY          5

#define SILICONFLOW_API_KEY     "Bearer sk-pwacimgxvzjxwyrzygqwiyhydzflwvgwttfasqfvhgozrrdr"
#define HTTP_KEEP_ALIVE_TIME    30

// ==================== AI 聊天伴侣配置 ====================
#define AI_AGENT_API_URL                "https://api.deepseek.com/chat/completions"
#define AI_AGENT_API_KEY                "Bearer sk-aeb22f22a73c4417bbadd3b32b6a1d0c"
#define AI_AGENT_MODEL                  "deepseek-chat"
#define AI_AGENT_HTTP_TIMEOUT_MS        30000
#define AI_AGENT_RESPONSE_TEXT_MAX_LEN  1024
#define AI_AGENT_HTTP_RESPONSE_MAX_LEN  8192
#define AI_AGENT_SYSTEM_PROMPT          "你是小吕，是一个中文聊天伴侣。请用自然、简短、温暖的中文回答用户。"

// ==================== AI 记忆配置 ====================
#define AI_AGENT_ENABLE_MEMORY              1
#define AI_AGENT_MAX_SHORT_HISTORY          20
#define AI_AGENT_SUMMARY_INTERVAL           10
#define AI_AGENT_MEMORY_NVS_NAMESPACE       "ai_agent"
#define AI_AGENT_SUMMARY_PROMPT             "请用100字以内总结以下对话的关键信息和用户偏好："
#define AI_AGENT_MEMORY_MAX_SUMMARY_LEN     512