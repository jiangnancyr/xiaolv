#ifndef DRIVER_TESTS_H
#define DRIVER_TESTS_H

#include "esp_err.h"

esp_err_t driver_test_es8311(void);
esp_err_t driver_test_es8311_audio(void);
esp_err_t driver_test_audio_workflow(void);
esp_err_t driver_test_sc7a20(void);
esp_err_t driver_test_ns4150(void);
esp_err_t driver_test_w25q128(void);
esp_err_t driver_test_run_all(void);

#endif /* DRIVER_TESTS_H */
