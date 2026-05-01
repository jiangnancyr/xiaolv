#include "es8311_audio.h"
#include "example_config.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_codec_dev_defaults.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_vol.h"
#include "esp_check.h"
#include "esp_log.h"

#define TAG "ES8311_AUDIO"

static i2s_chan_handle_t s_tx = NULL;
static i2s_chan_handle_t s_rx = NULL;
static i2c_master_bus_handle_t s_i2c_bus = NULL;
static esp_codec_dev_handle_t s_codec = NULL;
static bool s_inited = false;


esp_err_t es8311_audio_init(const es8311_audio_cfg_t *cfg)
{
    esp_err_t ret = ESP_OK;

    if (!cfg) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_inited) {
        return ESP_OK;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &s_tx, &s_rx), TAG, "i2s channel create failed");

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(cfg->sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_MCK_IO,
            .bclk = I2S_BCK_IO,
            .ws = I2S_WS_IO,
            .dout = I2S_DO_IO,
            .din = I2S_DI_IO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    std_cfg.clk_cfg.mclk_multiple = EXAMPLE_MCLK_MULTIPLE;

    ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(s_tx, &std_cfg), err, TAG, "tx std mode init failed");
    ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(s_rx, &std_cfg), err, TAG, "rx std mode init failed");
    ESP_GOTO_ON_ERROR(i2s_channel_enable(s_tx), err, TAG, "tx enable failed");
    ESP_GOTO_ON_ERROR(i2s_channel_enable(s_rx), err, TAG, "rx enable failed");

    i2c_master_bus_config_t i2c_cfg = {
        .i2c_port = I2C_NUM,
        .sda_io_num = I2C_SDA_IO,
        .scl_io_num = I2C_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_GOTO_ON_ERROR(i2c_new_master_bus(&i2c_cfg, &s_i2c_bus), err, TAG, "i2c init failed");

    audio_codec_i2c_cfg_t codec_i2c_cfg = {
        .port = I2C_NUM,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = s_i2c_bus,
    };
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&codec_i2c_cfg);
    ESP_GOTO_ON_FALSE(ctrl_if != NULL, ESP_FAIL, err, TAG, "codec ctrl create failed");

    audio_codec_i2s_cfg_t codec_i2s_cfg = {
        .port = I2S_NUM,
        .rx_handle = s_rx,
        .tx_handle = s_tx,
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&codec_i2s_cfg);
    ESP_GOTO_ON_FALSE(data_if != NULL, ESP_FAIL, err, TAG, "codec i2s data if create failed");

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    ESP_GOTO_ON_FALSE(gpio_if != NULL, ESP_FAIL, err, TAG, "codec gpio if create failed");

    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
        .master_mode = false,
        .use_mclk = I2S_MCK_IO >= 0,
        .pa_pin = -1,
        .pa_reverted = false,
        .hw_gain = {
            .pa_voltage = 5.0,
            .codec_dac_voltage = 3.3,
        },
        .mclk_div = EXAMPLE_MCLK_MULTIPLE,
    };
    const audio_codec_if_t *codec_if = es8311_codec_new(&es8311_cfg);
    ESP_GOTO_ON_FALSE(codec_if != NULL, ESP_FAIL, err, TAG, "es8311 codec if create failed");

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,
        .codec_if = codec_if,
        .data_if = data_if,
    };
    s_codec = esp_codec_dev_new(&dev_cfg);
    ESP_GOTO_ON_FALSE(s_codec != NULL, ESP_FAIL, err, TAG, "codec dev create failed");

    esp_codec_dev_sample_info_t sample_cfg = {
        .bits_per_sample = cfg->bits_per_sample,
        .channel = cfg->channels,
        .channel_mask = (cfg->channels == 2) ? 0x03 : 0x01,
        .sample_rate = cfg->sample_rate,
    };
    ESP_GOTO_ON_FALSE(esp_codec_dev_open(s_codec, &sample_cfg) == ESP_CODEC_DEV_OK, ESP_FAIL, err, TAG, "codec open failed");
    ESP_GOTO_ON_FALSE(esp_codec_dev_set_out_vol(s_codec, cfg->out_vol) == ESP_CODEC_DEV_OK, ESP_FAIL, err, TAG, "set out vol failed");
    ESP_GOTO_ON_FALSE(esp_codec_dev_set_in_gain(s_codec, cfg->in_gain_db) == ESP_CODEC_DEV_OK, ESP_FAIL, err, TAG, "set in gain failed");

    s_inited = true;
    return ESP_OK;

err:
    (void)es8311_audio_deinit();
    return ESP_FAIL;
}

esp_err_t es8311_audio_deinit(void)
{
    if (s_tx) {
        (void)i2s_channel_disable(s_tx);
    }
    if (s_rx) {
        (void)i2s_channel_disable(s_rx);
    }
    if (s_tx || s_rx) {
        (void)i2s_del_channel(s_tx);
        (void)i2s_del_channel(s_rx);
        s_tx = NULL;
        s_rx = NULL;
    }
    if (s_i2c_bus) {
        (void)i2c_del_master_bus(s_i2c_bus);
        s_i2c_bus = NULL;
    }
    s_codec = NULL;
    s_inited = false;
    return ESP_OK;
}

esp_err_t es8311_audio_output(const void *pcm, size_t len, size_t *bytes_written, TickType_t timeout_ticks)
{
    if (!s_inited || !s_tx || !pcm || len == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2s_channel_write(s_tx, pcm, len, bytes_written, timeout_ticks);
}

esp_err_t es8311_audio_input(void *pcm, size_t len, size_t *bytes_read, TickType_t timeout_ticks)
{
    if (!s_inited || !s_rx || !pcm || len == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2s_channel_read(s_rx, pcm, len, bytes_read, timeout_ticks);
}
