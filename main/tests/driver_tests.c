#include "driver_tests.h"
#include "example_config.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "driver/i2s_std.h"
#include "mw_i2c_bus.h"
#include "es8311_board.h"
#include "es8311_audio.h"
#include "audio_workflow.h"
#include "task_scheduler.h"
#include "workflow.h"
#include "sc7a20.h"
#include "ns4150.h"
#include "w25q128.h"
#include "ai_agent_tests.h"

#define TAG "DRV_TEST"

static esp_err_t create_test_i2c(i2c_master_bus_handle_t *bus)
{
    mw_i2c_bus_cfg_t cfg = {
        .port = CONFIG_MW_TEST_I2C_PORT,
        .sda_io = CONFIG_MW_TEST_I2C_SDA_IO,
        .scl_io = CONFIG_MW_TEST_I2C_SCL_IO,
        .clk_hz = 400000,
        .enable_internal_pullup = true,
    };
    return mw_i2c_bus_create(&cfg, bus);
}

esp_err_t driver_test_es8311(void)
{
    i2c_master_bus_handle_t bus = NULL;
    ESP_RETURN_ON_ERROR(create_test_i2c(&bus), TAG, "create i2c bus failed");

    esp_err_t err = es8311_board_probe(bus);
    if (err == ESP_OK) {
        uint8_t id = 0;
        err = es8311_board_read_chip_id(bus, &id);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "ES8311 chip id: 0x%02X", id);
        }
    }

    (void)mw_i2c_bus_delete(bus);
    return err;
}

esp_err_t driver_test_es8311_audio(void)
{
    es8311_audio_cfg_t cfg = {
        .sample_rate = 16000,
        .bits_per_sample = I2S_DATA_BIT_WIDTH_16BIT,
        .channels = 1,
        .out_vol = 60,
        .in_gain_db = 18,
    };
    ESP_RETURN_ON_ERROR(es8311_audio_init(&cfg), TAG, "es8311 audio init failed");

    int16_t buf[256] = {0};
    size_t br = 0;
    size_t bw = 0;
    esp_err_t err = es8311_audio_input(buf, sizeof(buf), &br, pdMS_TO_TICKS(500));
    if (err == ESP_OK && br > 0) {
        err = es8311_audio_output(buf, br, &bw, pdMS_TO_TICKS(500));
    }

    (void)es8311_audio_deinit();
    return err;
}

esp_err_t driver_test_audio_workflow(void)
{
    audio_wf_ctx_t ctx = {0};
    es8311_audio_cfg_t cfg = {
        .sample_rate = 16000,
        .bits_per_sample = I2S_DATA_BIT_WIDTH_16BIT,
        .channels = 1,
        .out_vol = 60,
        .in_gain_db = 18,
    };
    ESP_RETURN_ON_ERROR(audio_wf_ctx_init(&ctx, &cfg, 1024, pdMS_TO_TICKS(100)), TAG, "audio_wf_ctx_init failed");

    const wf_def_t *def = audio_wf_get_loopback_def(&ctx);
    if (!def) {
        (void)audio_wf_ctx_deinit(&ctx);
        return ESP_FAIL;
    }

    wf_runtime_t rt = {0};
    esp_err_t err = wf_runtime_init(&rt, def);
    if (err == ESP_OK) {
        err = wf_runtime_run_once(&rt);
        (void)wf_runtime_deinit(&rt);
    }

    (void)audio_wf_ctx_deinit(&ctx);
    return err;
}

esp_err_t driver_test_sc7a20(void)
{
    i2c_master_bus_handle_t bus = NULL;
    ESP_RETURN_ON_ERROR(create_test_i2c(&bus), TAG, "create i2c bus failed");

    esp_err_t err = sc7a20_probe(bus);
    if (err == ESP_OK) {
        uint8_t who = 0;
        err = sc7a20_read_whoami(bus, &who);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "SC7A20 whoami: 0x%02X", who);
            (void)sc7a20_enable_basic(bus);
        }
    }

    (void)mw_i2c_bus_delete(bus);
    return err;
}

esp_err_t driver_test_ns4150(void)
{
    ns4150_cfg_t cfg = {
        .ctrl_io = CONFIG_MW_NS4150_CTRL_IO,
        .active_high = CONFIG_MW_NS4150_ACTIVE_HIGH,
    };
    ESP_RETURN_ON_ERROR(ns4150_init(&cfg), TAG, "ns4150 init failed");
    ESP_RETURN_ON_ERROR(ns4150_set_enable(true), TAG, "ns4150 enable failed");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(ns4150_set_enable(false), TAG, "ns4150 disable failed");
    return ESP_OK;
}

esp_err_t driver_test_w25q128(void)
{
    w25q128_cfg_t cfg = {
        .host = (spi_host_device_t)CONFIG_MW_W25Q128_SPI_HOST,
        .mosi_io = CONFIG_MW_W25Q128_MOSI_IO,
        .miso_io = CONFIG_MW_W25Q128_MISO_IO,
        .sclk_io = CONFIG_MW_W25Q128_SCLK_IO,
        .cs_io = CONFIG_MW_W25Q128_CS_IO,
        .clock_hz = CONFIG_MW_W25Q128_SPI_HZ,
    };
    w25q128_handle_t h = {0};
    ESP_RETURN_ON_ERROR(w25q128_init(&cfg, &h), TAG, "w25q128 init failed");

    uint8_t mid = 0, mem_type = 0, cap = 0;
    esp_err_t err = w25q128_read_jedec_id(&h, &mid, &mem_type, &cap);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "W25Q128 JEDEC ID: %02X %02X %02X", mid, mem_type, cap);
    }

    (void)w25q128_deinit(&h);
    return err;
}

esp_err_t driver_test_run_all(void)
{
    esp_err_t err = ESP_OK;

    ESP_LOGI(TAG, "==== Driver tests begin ====");

    // err = driver_test_es8311();
    // ESP_LOGI(TAG, "ES8311 test: %s", esp_err_to_name(err));

    // err = driver_test_sc7a20();
    // ESP_LOGI(TAG, "SC7A20 test: %s", esp_err_to_name(err));

    // err = driver_test_es8311_audio();
    // ESP_LOGI(TAG, "ES8311 audio test: %s", esp_err_to_name(err));

    // err = driver_test_audio_workflow();
    // ESP_LOGI(TAG, "Audio workflow test: %s", esp_err_to_name(err));

    // err = driver_test_ns4150();
    // ESP_LOGI(TAG, "NS4150 test: %s", esp_err_to_name(err));

    // err = driver_test_w25q128();
    // ESP_LOGI(TAG, "W25Q128 test: %s", esp_err_to_name(err));

    task_config_t ai_task_config = {
        .name = "ai task",
        .task_func = ai_agent_test_run_all,
        .params = NULL,
        .stack_size = TASK_STACK_AI,
        .priority = TASK_PRIORITY_HIGH,
        .core_id = -1
    };
    ESP_ERROR_CHECK(task_create(&ai_task_config));

    ESP_LOGI(TAG, "==== Driver tests end ====");
    return ESP_OK;
}
