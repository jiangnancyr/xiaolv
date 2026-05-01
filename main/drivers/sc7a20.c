#include "sc7a20.h"
#include "mw_i2c_bus.h"
#include "mw_board_config.h"

esp_err_t sc7a20_probe(i2c_master_bus_handle_t bus)
{
    return mw_i2c_probe(bus, MW_SC7A20_I2C_ADDR, 20);
}

esp_err_t sc7a20_read_whoami(i2c_master_bus_handle_t bus, uint8_t *whoami)
{
    if (!whoami) {
        return ESP_ERR_INVALID_ARG;
    }
    return mw_i2c_read_reg(bus, MW_SC7A20_I2C_ADDR, MW_SC7A20_REG_WHO_AM_I, whoami, 1, 20);
}

esp_err_t sc7a20_enable_basic(i2c_master_bus_handle_t bus)
{
    /* CTRL1: 100Hz + XYZ enable */
    uint8_t ctrl1 = 0x57;
    esp_err_t err = mw_i2c_write_reg(bus, MW_SC7A20_I2C_ADDR, MW_SC7A20_REG_CTRL1, &ctrl1, 1, 20);
    if (err != ESP_OK) {
        return err;
    }

    /* CTRL4: high-resolution + +/-2g */
    uint8_t ctrl4 = 0x08;
    return mw_i2c_write_reg(bus, MW_SC7A20_I2C_ADDR, MW_SC7A20_REG_CTRL4, &ctrl4, 1, 20);
}

esp_err_t sc7a20_read_xyz_raw(i2c_master_bus_handle_t bus, int16_t *x, int16_t *y, int16_t *z)
{
    if (!x || !y || !z) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t raw[6] = {0};
    /* 自动地址递增读，常见寄存器位7置1 */
    esp_err_t err = mw_i2c_read_reg(bus, MW_SC7A20_I2C_ADDR, (uint8_t)(MW_SC7A20_REG_OUT_X_L | 0x80), raw, 6, 20);
    if (err != ESP_OK) {
        return err;
    }

    *x = (int16_t)((raw[1] << 8) | raw[0]);
    *y = (int16_t)((raw[3] << 8) | raw[2]);
    *z = (int16_t)((raw[5] << 8) | raw[4]);
    return ESP_OK;
}
