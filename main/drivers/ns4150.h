#ifndef NS4150_H
#define NS4150_H

#include "esp_err.h"
#include "driver/gpio.h"

typedef struct {
    gpio_num_t ctrl_io; /* 放大器 CTRL 引脚，<0 表示不使用 GPIO 控制 */
    bool active_high;
} ns4150_cfg_t;

esp_err_t ns4150_init(const ns4150_cfg_t *cfg);
esp_err_t ns4150_set_enable(bool enable);

#endif /* NS4150_H */
