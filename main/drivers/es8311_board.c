#include "es8311_board.h"
#include "mw_i2c_bus.h"
#include "mw_board_config.h"

/* ES8311 常见设备ID寄存器地址（不同版本可能差异，本测试只用于基本连通性检查） */
#define ES8311_REG_CHIP_ID 0xFD

esp_err_t es8311_board_probe(i2c_master_bus_handle_t bus)
{
    return mw_i2c_probe(bus, MW_ES8311_I2C_ADDR, 20);
}

esp_err_t es8311_board_read_chip_id(i2c_master_bus_handle_t bus, uint8_t *chip_id)
{
    if (!chip_id) {
        return ESP_ERR_INVALID_ARG;
    }
    return mw_i2c_read_reg(bus, MW_ES8311_I2C_ADDR, ES8311_REG_CHIP_ID, chip_id, 1, 20);
}
