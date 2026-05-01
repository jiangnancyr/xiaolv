#include "w25q128.h"
#include <string.h>
#include "mw_board_config.h"

esp_err_t w25q128_init(const w25q128_cfg_t *cfg, w25q128_handle_t *out)
{
    if (!cfg || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->host = cfg->host;

    spi_bus_config_t buscfg = {
        .mosi_io_num = cfg->mosi_io,
        .miso_io_num = cfg->miso_io,
        .sclk_io_num = cfg->sclk_io,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 64,
    };

    esp_err_t err = spi_bus_initialize(cfg->host, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        return err;
    }
    out->bus_inited = true;

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = cfg->clock_hz > 0 ? cfg->clock_hz : 10000000,
        .mode = 0,
        .spics_io_num = cfg->cs_io,
        .queue_size = 1,
    };

    err = spi_bus_add_device(cfg->host, &devcfg, &out->dev);
    if (err != ESP_OK) {
        spi_bus_free(cfg->host);
        out->bus_inited = false;
        return err;
    }
    return ESP_OK;
}

esp_err_t w25q128_deinit(w25q128_handle_t *h)
{
    if (!h) {
        return ESP_ERR_INVALID_ARG;
    }
    if (h->dev) {
        (void)spi_bus_remove_device(h->dev);
        h->dev = NULL;
    }
    if (h->bus_inited) {
        (void)spi_bus_free(h->host);
        h->bus_inited = false;
    }
    return ESP_OK;
}

esp_err_t w25q128_read_jedec_id(w25q128_handle_t *h, uint8_t *mid, uint8_t *mem_type, uint8_t *capacity)
{
    if (!h || !h->dev || !mid || !mem_type || !capacity) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t tx[4] = {MW_W25Q128_CMD_JEDEC_ID, 0, 0, 0};
    uint8_t rx[4] = {0};

    spi_transaction_t t = {
        .length = 8 * sizeof(tx),
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    esp_err_t err = spi_device_transmit(h->dev, &t);
    if (err != ESP_OK) {
        return err;
    }

    *mid = rx[1];
    *mem_type = rx[2];
    *capacity = rx[3];
    return ESP_OK;
}
