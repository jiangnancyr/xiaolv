#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "workflow.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TM_MAX_TASKS
#define TM_MAX_TASKS WF_MAX_TASKS
#endif

#ifndef TM_MAX_STEPS
#define TM_MAX_STEPS 8
#endif

typedef enum {
    TM_STEP_SERIAL = 0,
    TM_STEP_PARALLEL = 1,
} tm_step_type_t;

typedef struct {
    const char *name;
    wf_task_fn_t fn;
    void *arg;
    uint32_t stack_size;
    UBaseType_t priority;
    BaseType_t core_id;
} tm_task_config_t;

typedef struct {
    char name[32];
    wf_task_desc_t tasks[TM_MAX_TASKS];
    uint8_t task_count;
    wf_step_desc_t steps[TM_MAX_STEPS];
    uint8_t step_task_ids[TM_MAX_STEPS][TM_MAX_TASKS];
    uint8_t step_count;
    wf_def_t def;
    wf_runtime_t runtime;
    bool runtime_inited;
} task_manager_t;

esp_err_t task_manager_init(task_manager_t *manager, const char *name);
esp_err_t task_manager_deinit(task_manager_t *manager);
esp_err_t task_manager_add_task(task_manager_t *manager, uint8_t task_id, const tm_task_config_t *config);
esp_err_t task_manager_add_step(task_manager_t *manager,
                                const char *name,
                                tm_step_type_t type,
                                const uint8_t *task_ids,
                                uint8_t task_count);
esp_err_t task_manager_build(task_manager_t *manager, const wf_def_t **out_def);
esp_err_t task_manager_start(task_manager_t *manager);
esp_err_t task_manager_run_once(task_manager_t *manager);
esp_err_t task_manager_stop(task_manager_t *manager);
const wf_def_t *task_manager_get_def(const task_manager_t *manager);
wf_runtime_t *task_manager_get_runtime(task_manager_t *manager);

#ifdef __cplusplus
}
#endif

#endif /* TASK_MANAGER_H */
