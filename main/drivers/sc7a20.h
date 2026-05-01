#ifndef SC7A20_H
#define SC7A20_H

#include "esp_err.h"
#include "driver/i2c_master.h"

esp_err_t sc7a20_probe(i2c_master_bus_handle_t bus);
esp_err_t sc7a20_read_whoami(i2c_master_bus_handle_t bus, uint8_t *whoami);
esp_err_t sc7a20_enable_basic(i2c_master_bus_handle_t bus);
esp_err_t sc7a20_read_xyz_raw(i2c_master_bus_handle_t bus, int16_t *x, int16_t *y, int16_t *z);

#endif /* SC7A20_H */
