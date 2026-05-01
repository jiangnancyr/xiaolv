// main/utils/task_scheduler.h
#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

typedef struct {
    const char *name;
    TaskFunction_t task_func;
    void *params;
    uint32_t stack_size;
    UBaseType_t priority;
    TaskHandle_t handle;
    BaseType_t core_id;  // -1表示不指定核心
} task_config_t;

// 初始化任务调度器
esp_err_t task_scheduler_init(void);

// 创建任务
esp_err_t task_create(task_config_t *config);

// 删除任务
esp_err_t task_delete(const char *name);

// 暂停任务
esp_err_t task_suspend(const char *name);

// 恢复任务
esp_err_t task_resume(const char *name);

// 获取任务状态
esp_err_t task_get_info(const char *name, TaskStatus_t *status);

// 获取系统任务统计
void task_print_stats(void);

// 创建定时器
TimerHandle_t task_create_timer(const char *name, 
                                 TickType_t period,
                                 bool auto_reload,
                                 TimerCallbackFunction_t callback,
                                 void *arg);

#endif // TASK_SCHEDULER_H