#ifndef TASK__BUILD__H
#define TASK__BUILD__H

#include "esp_err.h"
esp_err_t start_chat_flow(void);
void chat_flow_manager_task(void *pvParameters);

#endif 