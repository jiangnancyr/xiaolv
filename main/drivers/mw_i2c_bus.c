#include "mw_i2c_bus.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#define TAG "MW_I2C"

esp_err_t mw_i2c_bus_create(const mw_i2c_bus_cfg_t *cfg, i2c_master_bus_handle_t *out_bus)
{
    if (!cfg || !out_bus) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = cfg->port,
        .sda_io_num = cfg->sda_io,
        .scl_io_num = cfg->scl_io,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = cfg->enable_internal_pullup,
    };

    esp_err_t err = i2c_new_master_bus(&bus_cfg, out_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t mw_i2c_bus_delete(i2c_master_bus_handle_t bus)
{
    if (!bus) {
        return ESP_ERR_INVALID_ARG;
    }
    return i2c_del_master_bus(bus);
}

esp_err_t mw_i2c_probe(i2c_master_bus_handle_t bus, uint8_t addr_7bit, uint32_t timeout_ms)
{
    if (!bus) {
        return ESP_ERR_INVALID_ARG;
    }
    return i2c_master_probe(bus, addr_7bit, pdMS_TO_TICKS(timeout_ms));
}

esp_err_t mw_i2c_read_reg(i2c_master_bus_handle_t bus, uint8_t addr_7bit, uint8_t reg,
                          uint8_t *data, size_t len, uint32_t timeout_ms)
{
    if (!bus || !data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr_7bit,
        .scl_speed_hz = 400000,
    };

    i2c_master_dev_handle_t dev = NULL;
    esp_err_t err = i2c_master_bus_add_device(bus, &dev_cfg, &dev);
    if (err != ESP_OK) {
        return err;
    }

    err = i2c_master_transmit_receive(dev, &reg, 1, data, len, pdMS_TO_TICKS(timeout_ms));
    (void)i2c_master_bus_rm_device(dev);
    return err;
}

esp_err_t mw_i2c_write_reg(i2c_master_bus_handle_t bus, uint8_t addr_7bit, uint8_t reg,
                           const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    if (!bus || (!data && len > 0)) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr_7bit,
        .scl_speed_hz = 400000,
    };

    i2c_master_dev_handle_t dev = NULL;
    esp_err_t err = i2c_master_bus_add_device(bus, &dev_cfg, &dev);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t buf[32];
    if (len + 1 > sizeof(buf)) {
        (void)i2c_master_bus_rm_device(dev);
        return ESP_ERR_INVALID_SIZE;
    }

    buf[0] = reg;
    if (len > 0) {
        memcpy(&buf[1], data, len);
    }
    err = i2c_master_transmit(dev, buf, len + 1, pdMS_TO_TICKS(timeout_ms));
    (void)i2c_master_bus_rm_device(dev);
    return err;
}
