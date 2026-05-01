#ifndef W25Q128_H
#define W25Q128_H

#include "esp_err.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

typedef struct {
    spi_host_device_t host;
    gpio_num_t mosi_io;
    gpio_num_t miso_io;
    gpio_num_t sclk_io;
    gpio_num_t cs_io;
    int clock_hz;
} w25q128_cfg_t;

typedef struct {
    spi_device_handle_t dev;
    spi_host_device_t host;
    bool bus_inited;
} w25q128_handle_t;

esp_err_t w25q128_init(const w25q128_cfg_t *cfg, w25q128_handle_t *out);
esp_err_t w25q128_deinit(w25q128_handle_t *h);
esp_err_t w25q128_read_jedec_id(w25q128_handle_t *h, uint8_t *mid, uint8_t *mem_type, uint8_t *capacity);

#endif /* W25Q128_H */
