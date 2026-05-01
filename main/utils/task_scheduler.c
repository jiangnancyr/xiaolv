// main/utils/task_scheduler.c
#include "task_scheduler.h"
#include "logger.h"
#include <string.h>
#include <sdkconfig.h>

#define TAG "SCHEDULER"

#define MAX_TASKS 20

typedef struct {
    char name[32];
    TaskHandle_t handle;
    bool active;
} task_record_t;

static task_record_t g_tasks[MAX_TASKS];
static SemaphoreHandle_t g_task_mutex = NULL;

static task_record_t* find_task(const char *name)
{
    for (int i = 0; i < MAX_TASKS; i++) {
        if (g_tasks[i].active && strcmp(g_tasks[i].name, name) == 0) {
            return &g_tasks[i];
        }
    }
    return NULL;
}

static int find_empty_slot(void)
{
    for (int i = 0; i < MAX_TASKS; i++) {
        if (!g_tasks[i].active) {
            return i;
        }
    }
    return -1;
}

esp_err_t task_scheduler_init(void)
{
    memset(g_tasks, 0, sizeof(g_tasks));
    g_task_mutex = xSemaphoreCreateMutex();
    if (!g_task_mutex) {
        LOG_E(TAG, "Failed to create task mutex");
        return ESP_FAIL;
    }
    
    LOG_I(TAG, "Task scheduler initialized");
    return ESP_OK;
}

esp_err_t task_create(task_config_t *config)
{
    if (!config || !config->name || !config->task_func) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(g_task_mutex, portMAX_DELAY);
    
    // 检查是否已存在同名任务
    if (find_task(config->name)) {
        LOG_E(TAG, "Task %s already exists", config->name);
        xSemaphoreGive(g_task_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    int slot = find_empty_slot();
    if (slot < 0) {
        LOG_E(TAG, "No available task slot");
        xSemaphoreGive(g_task_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    TaskHandle_t handle = NULL;
    BaseType_t result;
    
    if (config->core_id >= 0 && config->core_id < CONFIG_FREERTOS_NUMBER_OF_CORES) {
        // 指定核心创建任务
        result = xTaskCreatePinnedToCore(
            config->task_func,
            config->name,
            config->stack_size,
            config->params,
            config->priority,
            &handle,
            config->core_id
        );
    } else {
        // 不指定核心
        result = xTaskCreate(
            config->task_func,
            config->name,
            config->stack_size,
            config->params,
            config->priority,
            &handle
        );
    }
    
    if (result != pdPASS) {
        LOG_E(TAG, "Failed to create task %s", config->name);
        xSemaphoreGive(g_task_mutex);
        return ESP_FAIL;
    }
    
    // 记录任务信息
    strncpy(g_tasks[slot].name, config->name, sizeof(g_tasks[slot].name) - 1);
    g_tasks[slot].handle = handle;
    g_tasks[slot].active = true;
    
    xSemaphoreGive(g_task_mutex);
    
    LOG_I(TAG, "Task %s created (priority: %d, stack: %d)", 
          config->name, config->priority, config->stack_size);
    
    return ESP_OK;
}

esp_err_t task_delete(const char *name)
{
    if (!name) return ESP_ERR_INVALID_ARG;
    
    xSemaphoreTake(g_task_mutex, portMAX_DELAY);
    
    task_record_t *task = find_task(name);
    if (!task) {
        LOG_E(TAG, "Task %s not found", name);
        xSemaphoreGive(g_task_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    vTaskDelete(task->handle);
    task->active = false;
    
    xSemaphoreGive(g_task_mutex);
    
    LOG_I(TAG, "Task %s deleted", name);
    return ESP_OK;
}

esp_err_t task_suspend(const char *name)
{
    if (!name) return ESP_ERR_INVALID_ARG;
    
    xSemaphoreTake(g_task_mutex, portMAX_DELAY);
    
    task_record_t *task = find_task(name);
    if (!task) {
        xSemaphoreGive(g_task_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    vTaskSuspend(task->handle);
    
    xSemaphoreGive(g_task_mutex);
    
    LOG_D(TAG, "Task %s suspended", name);
    return ESP_OK;
}

esp_err_t task_resume(const char *name)
{
    if (!name) return ESP_ERR_INVALID_ARG;
    
    xSemaphoreTake(g_task_mutex, portMAX_DELAY);
    
    task_record_t *task = find_task(name);
    if (!task) {
        xSemaphoreGive(g_task_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    vTaskResume(task->handle);
    
    xSemaphoreGive(g_task_mutex);
    
    LOG_D(TAG, "Task %s resumed", name);
    return ESP_OK;
}

// 检查 FreeRTOS 统计功能是否启用
#if (configUSE_TRACE_FACILITY == 1) && (configUSE_STATS_FORMATTING_FUNCTIONS == 1)

void task_print_stats(void)
{
    TaskStatus_t *task_status;
    UBaseType_t num_tasks = uxTaskGetNumberOfTasks();
    
    task_status = malloc(num_tasks * sizeof(TaskStatus_t));
    if (!task_status) {
        LOG_E(TAG, "Failed to allocate memory for task stats");
        return;
    }
    
    // 获取系统任务状态 - 这个函数现在可用
    num_tasks = uxTaskGetSystemState(task_status, num_tasks, NULL);
    
    LOG_I(TAG, "========== Task Statistics ==========");
    LOG_I(TAG, "%-20s %-8s %-12s %-12s", 
          "Task Name", "Priority", "Stack High Water", "State");
    
    for (int i = 0; i < num_tasks; i++) {
        const char *state_str;
        switch (task_status[i].eCurrentState) {
            case eRunning:   state_str = "Running"; break;
            case eReady:     state_str = "Ready"; break;
            case eBlocked:   state_str = "Blocked"; break;
            case eSuspended: state_str = "Suspended"; break;
            case eDeleted:   state_str = "Deleted"; break;
            default:         state_str = "Unknown"; break;
        }
        
        LOG_I(TAG, "%-20s %-8d %-12d %-12s", 
              task_status[i].pcTaskName,
              task_status[i].uxCurrentPriority,
              task_status[i].usStackHighWaterMark * 4,
              state_str);
    }
    LOG_I(TAG, "=====================================");
    
    free(task_status);
}

#else

// 如果统计功能未启用，提供简化版本
void task_print_stats(void)
{
    LOG_W(TAG, "Task statistics not enabled. Enable CONFIG_FREERTOS_USE_TRACE_FACILITY and CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS");
    
    // 简化版本：只打印任务数量
    UBaseType_t num_tasks = uxTaskGetNumberOfTasks();
    LOG_I(TAG, "Total tasks: %d (stats disabled)", num_tasks);
    
    // 使用 vTaskList 作为替代（如果可用）
    #if (configUSE_TRACE_FACILITY == 1)
    char *task_list = malloc(512);
    if (task_list) {
        vTaskList(task_list);
        LOG_I(TAG, "Task list:\n%s", task_list);
        free(task_list);
    }
    #endif
}

#endif

TimerHandle_t task_create_timer(const char *name, 
                                 TickType_t period,
                                 bool auto_reload,
                                 TimerCallbackFunction_t callback,
                                 void *arg)
{
    TimerHandle_t timer = xTimerCreate(
        name,
        period,
        auto_reload ? pdTRUE : pdFALSE,
        arg,
        callback
    );
    
    if (timer) {
        LOG_D(TAG, "Timer %s created", name);
    } else {
        LOG_E(TAG, "Failed to create timer %s", name);
    }
    
    return timer;
}