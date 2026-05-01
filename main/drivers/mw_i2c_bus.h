#ifndef MW_I2C_BUS_H
#define MW_I2C_BUS_H

#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

/* I2C 总线配置 */
typedef struct {
    i2c_port_num_t port;
    gpio_num_t sda_io;
    gpio_num_t scl_io;
    uint32_t clk_hz;
    bool enable_internal_pullup;
} mw_i2c_bus_cfg_t;

/* 创建设备总线句柄 */
esp_err_t mw_i2c_bus_create(const mw_i2c_bus_cfg_t *cfg, i2c_master_bus_handle_t *out_bus);

/* 删除设备总线句柄 */
esp_err_t mw_i2c_bus_delete(i2c_master_bus_handle_t bus);

/* 探测目标地址是否有设备应答 */
esp_err_t mw_i2c_probe(i2c_master_bus_handle_t bus, uint8_t addr_7bit, uint32_t timeout_ms);

/* 读寄存器 */
esp_err_t mw_i2c_read_reg(i2c_master_bus_handle_t bus, uint8_t addr_7bit, uint8_t reg,
                          uint8_t *data, size_t len, uint32_t timeout_ms);

/* 写寄存器 */
esp_err_t mw_i2c_write_reg(i2c_master_bus_handle_t bus, uint8_t addr_7bit, uint8_t reg,
                           const uint8_t *data, size_t len, uint32_t timeout_ms);

#endif /* MW_I2C_BUS_H */
