#ifndef ES8311_AUDIO_H
#define ES8311_AUDIO_H

#include <stddef.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

typedef struct {
    int sample_rate;
    int bits_per_sample;
    int channels;
    int out_vol;
    int in_gain_db;
} es8311_audio_cfg_t;

esp_err_t es8311_audio_init(const es8311_audio_cfg_t *cfg);
esp_err_t es8311_audio_deinit(void);
esp_err_t es8311_audio_output(const void *pcm, size_t len, size_t *bytes_written, TickType_t timeout_ticks);
esp_err_t es8311_audio_input(void *pcm, size_t len, size_t *bytes_read, TickType_t timeout_ticks);
esp_err_t es8311_audio_output_large(const void *pcm, size_t total_len, TickType_t timeout_ticks);

#endif /* ES8311_AUDIO_H */
