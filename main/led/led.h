#ifndef __LED__H
#define __LED__H

// LED状态定义
typedef enum {
    LED_OFF = 0,
    LED_ON = 1
} led_state_t;

esp_err_t led_init(void);


void led_set(led_state_t state);

// 翻转LED状态
void led_toggle(void);

// 获取当前LED状态
led_state_t led_get_state(void);

#endif 