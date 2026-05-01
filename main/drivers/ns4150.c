#include "ns4150.h"

static ns4150_cfg_t s_cfg = {
    .ctrl_io = -1,
    .active_high = true,
};
static bool s_inited = false;

esp_err_t ns4150_init(const ns4150_cfg_t *cfg)
{
    if (!cfg) {
        return ESP_ERR_INVALID_ARG;
    }
    s_cfg = *cfg;
    s_inited = true;

    if (s_cfg.ctrl_io >= 0) {
        gpio_config_t io_cfg = {
            .pin_bit_mask = 1ULL << s_cfg.ctrl_io,
            .mode = GPIO_MODE_OUTPUT,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        esp_err_t err = gpio_config(&io_cfg);
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}

esp_err_t ns4150_set_enable(bool enable)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_cfg.ctrl_io < 0) {
        return ESP_OK;
    }

    int level = (enable ? 1 : 0);
    if (!s_cfg.active_high) {
        level = !level;
    }
    return gpio_set_level(s_cfg.ctrl_io, level);
}
