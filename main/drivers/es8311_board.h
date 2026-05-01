#ifndef ES8311_BOARD_H
#define ES8311_BOARD_H

#include "esp_err.h"
#include "driver/i2c_master.h"

/* 仅做板级探测/基础寄存器读写，不替代 esp_codec_dev 的完整音频驱动 */
esp_err_t es8311_board_probe(i2c_master_bus_handle_t bus);
esp_err_t es8311_board_read_chip_id(i2c_master_bus_handle_t bus, uint8_t *chip_id);

#endif /* ES8311_BOARD_H */
