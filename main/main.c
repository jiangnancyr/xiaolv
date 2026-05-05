/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <string.h>
#include "wifi_manager.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_codec_dev_defaults.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_vol.h"
#include "esp_check.h"
#include "example_config.h"
#include "driver_tests.h"
#include "logger.h"
#include "task_scheduler.h"
#include <nvs_flash.h>
#include "led.h"
#include "task_build.h"
#include "cjson_init.h"
 static const char *TAG = "main";
// 给外放芯片供�?
static void PowerOnBoardPowerManager(void) 
{
    if (EXAMPLE_PA_CTRL_IO < 0) {
        return;
    }

    gpio_config_t gpio_init_struct = {0};

    gpio_init_struct.intr_type = GPIO_INTR_DISABLE;
    gpio_init_struct.mode = GPIO_MODE_INPUT_OUTPUT;
    gpio_init_struct.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_init_struct.pin_bit_mask = 1ull << EXAMPLE_PA_CTRL_IO;
    gpio_config(&gpio_init_struct);

    gpio_set_level(EXAMPLE_PA_CTRL_IO, 1);                      /* 打开音频电源 */
}

 static esp_err_t init_nvs(void)
 {
     esp_err_t ret = nvs_flash_init();
     if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
         ESP_LOGW(TAG, "NVS partition truncated, erasing...");
         ESP_ERROR_CHECK(nvs_flash_erase());
         ret = nvs_flash_init();
     }
     return ret;
 }
 
 // led任务示例
static void led_task(void *pvParameters)
{
    int count = 0;
    led_init();
    while (1) {
        led_toggle();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static esp_err_t do_wifi_connect(void)
{
    my_wifi_config_t wifi_config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
        .callback = NULL,
        .callback_arg = NULL,
        .max_retry = WIFI_MAX_RETRY
    };
    
    esp_err_t ret = wifi_connect(&wifi_config);
    if (ret == ESP_OK) {
        char ip[16];
        wifi_get_ip(ip, sizeof(ip));
        LOG_I(TAG, "Connected! IP: %s", ip);
    } else {
        LOG_E(TAG, "WiFi connection failed");
    }
    return ret;
}
// 在初始化 client 之前设置日志级别
void enable_http_debug(void) {
    // 启用 HTTP Client 的详细日�?
    esp_log_level_set("HTTP_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("HTTP_CLIENT_TRANSPORT", ESP_LOG_VERBOSE);
    
    // 可选：启用底层传输日志
    esp_log_level_set("TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TLS", ESP_LOG_VERBOSE);
}
void app_main(void)
{
    ESP_LOGI(TAG, "Internal free: %d bytes",
        (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI(TAG, "PSRAM free:    %d bytes",
        (int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    ESP_ERROR_CHECK(init_nvs());
    // 初始化日志系�?
    logger_init();
    cjson_setup();
    logger_set_level(LOG_LEVEL_DEBUG);
    LOG_I(TAG, "========================================");
    LOG_I(TAG, "%s v%s Starting...", "xiaolv", "1.0");
    LOG_I(TAG, "ESP32-S3 Voice Assistant System Framework");
    LOG_I(TAG, "========================================");
    PowerOnBoardPowerManager();
    // 初始化任务调度器
    ESP_ERROR_CHECK(task_scheduler_init());
    // // 初始化事件系�?
    // ESP_ERROR_CHECK(event_system_init());

        // 创建测试任务（演示用�?
    task_config_t led_task_config = {
        .name = "led_task",
        .task_func = led_task,
        .params = NULL,
        .stack_size = TASK_STACK_LED,
        .priority = TASK_PRIORITY_LOW,
        .core_id = -1
    };
    enable_http_debug();
    ESP_ERROR_CHECK(task_create(&led_task_config));

    ESP_LOGI(TAG, "Running board driver tests...");

    // 打开wifi
    // 初始化WiFi管理�?
    ESP_ERROR_CHECK(wifi_manager_init());
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_ERROR_CHECK(do_wifi_connect());
    // ESP_ERROR_CHECK(driver_test_run_all());
        task_config_t main_task_config = {
        .name = "chat_flow_manager_task",
        .task_func = chat_flow_manager_task,
        .params = NULL,
        .stack_size = TASK_STACK_MAIN,
        .priority = TASK_PRIORITY_HIGH,
        .core_id = -1
    };
    ESP_ERROR_CHECK(task_create(&main_task_config));
    while (1) {
        // 定期打印任务统计
        static int stats_counter = 0;
        if (stats_counter++ >= 60) {  // �?0秒打印一�?
            task_print_stats(); // 打印任务信息
            stats_counter = 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
 
