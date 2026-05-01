#ifndef AI_AGENT_TESTS_H
#define AI_AGENT_TESTS_H

#include "esp_err.h"

esp_err_t ai_agent_test_init_deinit(void);
esp_err_t ai_agent_test_online_chat(void);
void ai_agent_test_run_all(void *pvParameters);

#endif /* AI_AGENT_TESTS_H */
