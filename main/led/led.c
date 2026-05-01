#include "driver/gpio.h"
#include "esp_log.h"
#include "led.h"
#include "logger/logger.h"

#define LED_GPIO_PIN    GPIO_NUM_2
#define LED_TAG         "LED"

// 初始化LED GPIO
esp_err_t led_init(void) {
    gpio_config_t led_config = {
        .pin_bit_mask = (1ULL << LED_GPIO_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    ESP_ERROR_CHECK(gpio_config(&led_config));
    
    // 初始状态设置为关闭
    gpio_set_level(LED_GPIO_PIN, 0);
    
    LOG_I(LED_TAG, "LED GPIO%d initialized", LED_GPIO_PIN);
    return ESP_OK;
}

// 设置LED状态
void led_set(led_state_t state) {
    gpio_set_level(LED_GPIO_PIN, state);
    LOG_I(LED_TAG, "LED set to %s", state == LED_ON ? "ON" : "OFF");
}

// 翻转LED状态
void led_toggle(void) {
    static int current_level = LED_OFF;
    gpio_set_level(LED_GPIO_PIN, current_level);
    current_level = current_level ? LED_OFF : LED_ON;
    // LOG_I(LED_TAG, "LED toggled to %s", !current_level ? "ON" : "OFF");
}

// 获取当前LED状态
led_state_t led_get_state(void) {
    return (led_state_t)gpio_get_level(LED_GPIO_PIN);
}