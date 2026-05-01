#include "task_manager.h"
#include <string.h>
#include "logger.h"

#define TAG "TASK_MANAGER"
#define TM_DEFAULT_STACK_SIZE 4096
#define TM_DEFAULT_PRIORITY 5
#define TM_NO_CORE_PINNING (-1)

static void tm_copy_name(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    (void)strlcpy(dst, src, dst_size);
}

static int tm_find_task_index(const task_manager_t *manager, uint8_t task_id)
{
    if (!manager) {
        return -1;
    }

    for (int i = 0; i < manager->task_count; i++) {
        if (manager->tasks[i].id == task_id) {
            return i;
        }
    }
    return -1;
}

static esp_err_t tm_validate_step_tasks(const task_manager_t *manager, const uint8_t *task_ids, uint8_t task_count)
{
    if (!manager || !task_ids || task_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < task_count; i++) {
        if (tm_find_task_index(manager, task_ids[i]) < 0) {
            LOG_E(TAG, "Unknown task id %u in step", task_ids[i]);
            return ESP_ERR_NOT_FOUND;
        }
    }
    return ESP_OK;
}

esp_err_t task_manager_init(task_manager_t *manager, const char *name)
{
    if (!manager) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(manager, 0, sizeof(*manager));
    tm_copy_name(manager->name, sizeof(manager->name), name ? name : "task_flow");
    return ESP_OK;
}

esp_err_t task_manager_deinit(task_manager_t *manager)
{
    if (!manager) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = task_manager_stop(manager);
    memset(manager, 0, sizeof(*manager));
    return err;
}

esp_err_t task_manager_add_task(task_manager_t *manager, uint8_t task_id, const tm_task_config_t *config)
{
    if (!manager || !config || !config->fn) {
        return ESP_ERR_INVALID_ARG;
    }
    if (manager->runtime_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (manager->task_count >= TM_MAX_TASKS) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (tm_find_task_index(manager, task_id) >= 0) {
        return ESP_ERR_INVALID_STATE;
    }

    wf_task_desc_t *task = &manager->tasks[manager->task_count];
    task->id = task_id;
    task->name = config->name ? config->name : "unnamed_task";
    task->fn = config->fn;
    task->arg = config->arg;
    task->stack_size = config->stack_size > 0 ? config->stack_size : TM_DEFAULT_STACK_SIZE;
    task->priority = config->priority > 0 ? config->priority : TM_DEFAULT_PRIORITY;
    task->core_id = config->core_id;
    if (task->core_id < TM_NO_CORE_PINNING) {
        task->core_id = TM_NO_CORE_PINNING;
    }

    manager->task_count++;
    return ESP_OK;
}

esp_err_t task_manager_add_step(task_manager_t *manager,
                                const char *name,
                                tm_step_type_t type,
                                const uint8_t *task_ids,
                                uint8_t task_count)
{
    if (!manager || !task_ids || task_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (manager->runtime_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (manager->step_count >= TM_MAX_STEPS || task_count > TM_MAX_TASKS) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (type != TM_STEP_SERIAL && type != TM_STEP_PARALLEL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = tm_validate_step_tasks(manager, task_ids, task_count);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t step_index = manager->step_count;
    memcpy(manager->step_task_ids[step_index], task_ids, task_count * sizeof(uint8_t));

    manager->steps[step_index].type = (type == TM_STEP_PARALLEL) ? WF_STEP_PARALLEL : WF_STEP_SERIAL;
    manager->steps[step_index].task_ids = manager->step_task_ids[step_index];
    manager->steps[step_index].task_count = task_count;
    manager->steps[step_index].name = name ? name : "unnamed_step";
    manager->step_count++;
    return ESP_OK;
}

esp_err_t task_manager_build(task_manager_t *manager, const wf_def_t **out_def)
{
    if (!manager) {
        return ESP_ERR_INVALID_ARG;
    }
    if (manager->task_count == 0 || manager->step_count == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    for (int i = 0; i < manager->step_count; i++) {
        esp_err_t err = tm_validate_step_tasks(manager, manager->steps[i].task_ids, manager->steps[i].task_count);
        if (err != ESP_OK) {
            return err;
        }
    }

    manager->def.name = manager->name[0] ? manager->name : "task_flow";
    manager->def.tasks = manager->tasks;
    manager->def.task_count = manager->task_count;
    manager->def.steps = manager->steps;
    manager->def.step_count = manager->step_count;

    if (out_def) {
        *out_def = &manager->def;
    }
    return ESP_OK;
}

esp_err_t task_manager_start(task_manager_t *manager)
{
    if (!manager) {
        return ESP_ERR_INVALID_ARG;
    }
    if (manager->runtime_inited) {
        return ESP_OK;
    }

    esp_err_t err = task_manager_build(manager, NULL);
    if (err != ESP_OK) {
        return err;
    }

    err = wf_runtime_init(&manager->runtime, &manager->def);
    if (err != ESP_OK) {
        return err;
    }

    manager->runtime_inited = true;
    LOG_I(TAG, "Task manager started: %s", manager->def.name ? manager->def.name : "unnamed");
    return ESP_OK;
}

esp_err_t task_manager_run_once(task_manager_t *manager)
{
    if (!manager) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!manager->runtime_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    return wf_runtime_run_once(&manager->runtime);
}

esp_err_t task_manager_stop(task_manager_t *manager)
{
    if (!manager) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!manager->runtime_inited) {
        return ESP_OK;
    }

    esp_err_t err = wf_runtime_deinit(&manager->runtime);
    manager->runtime_inited = false;
    return err;
}

const wf_def_t *task_manager_get_def(const task_manager_t *manager)
{
    if (!manager || manager->task_count == 0 || manager->step_count == 0) {
        return NULL;
    }
    return &manager->def;
}

wf_runtime_t *task_manager_get_runtime(task_manager_t *manager)
{
    if (!manager || !manager->runtime_inited) {
        return NULL;
    }
    return &manager->runtime;
}
