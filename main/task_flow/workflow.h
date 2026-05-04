#ifndef WORKFLOW_H
#define WORKFLOW_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WF_MAX_TASKS
#define WF_MAX_TASKS 16
#endif

#ifndef WF_MAILBOX_LEN
#define WF_MAILBOX_LEN 8
#endif

/* 步骤执行类型：串行表示同一步内逐个任务执行；并行表示同一步内任务同时触发并等待全部完成 */
typedef enum {
    WF_STEP_SERIAL = 0,
    WF_STEP_PARALLEL = 1,
} wf_step_type_t;

/* 任务间消息结构：支持轻量消息ID、数值载荷和指针载荷 */
typedef struct {
    uint16_t msg_id;
    uint8_t from_task;
    uint8_t to_task;
    uint32_t value;
    void *ptr;
    bool is_last;
} wf_message_t;

struct wf_runtime;
/* 工作流任务函数签名：self_task_id 为当前任务ID，arg 为任务私有参数 */
typedef esp_err_t (*wf_task_fn_t)(struct wf_runtime *rt, uint8_t self_task_id, void *arg);

/* 单个任务描述：定义任务ID、函数、参数及 FreeRTOS 运行属性 */
typedef struct {
    uint8_t id;
    const char *name;
    wf_task_fn_t fn;
    void *arg;
    uint32_t stack_size;
    UBaseType_t priority;
    BaseType_t core_id; /* <0 means no core pinning */
} wf_task_desc_t;

/* 单个步骤描述：指定本步骤执行类型与包含的任务ID列表 */
typedef struct {
    wf_step_type_t type;
    const uint8_t *task_ids;
    uint8_t task_count;
    const char *name;
} wf_step_desc_t;

/* 工作流定义：由任务表和步骤表组成，描述一个完整流程 */
typedef struct {
    const char *name;
    const wf_task_desc_t *tasks;
    uint8_t task_count;
    const wf_step_desc_t *steps;
    uint8_t step_count;
} wf_def_t;

/* 工作流运行时对象：持有 worker 任务句柄、消息邮箱和完成队列 */
typedef struct wf_runtime {
    const wf_def_t *def;
    TaskHandle_t runner_task;
    TaskHandle_t worker_handles[WF_MAX_TASKS];
    QueueHandle_t mailbox[WF_MAX_TASKS];
    QueueHandle_t done_queue;
} wf_runtime_t;

typedef struct {
    uint8_t task_id;
    esp_err_t result;
} wf_task_done_msg_t;

/* ---------- definition macros ---------- */
/* 定义任务条目（建议在静态数组中使用） */
#define WF_TASK(_id, _name, _fn, _arg, _stack, _prio, _core) \
    { .id = (_id), .name = (_name), .fn = (_fn), .arg = (_arg), .stack_size = (_stack), .priority = (_prio), .core_id = (_core) }

/* 定义串行步骤：同一步中的任务按给定顺序逐个执行 */
#define WF_STEP_SERIAL_N(_name, ...) \
    { .type = WF_STEP_SERIAL, .task_ids = (const uint8_t[]){__VA_ARGS__}, .task_count = (uint8_t)(sizeof((const uint8_t[]){__VA_ARGS__}) / sizeof(uint8_t)), .name = (_name) }

/* 定义并行步骤：同一步中的任务会同时触发，并等待全部完成 */
#define WF_STEP_PARALLEL_N(_name, ...) \
    { .type = WF_STEP_PARALLEL, .task_ids = (const uint8_t[]){__VA_ARGS__}, .task_count = (uint8_t)(sizeof((const uint8_t[]){__VA_ARGS__}) / sizeof(uint8_t)), .name = (_name) }

/* 定义完整工作流：自动计算任务数和步骤数 */
#define WF_DEF(_name, _tasks, _steps) \
    { .name = (_name), .tasks = (_tasks), .task_count = (uint8_t)(sizeof(_tasks) / sizeof((_tasks)[0])), .steps = (_steps), .step_count = (uint8_t)(sizeof(_steps) / sizeof((_steps)[0])) }

/* ---------- runtime APIs ---------- */
/* 初始化工作流运行时，创建 worker、邮箱和完成队列 */
esp_err_t wf_runtime_init(wf_runtime_t *rt, const wf_def_t *def);
/* 反初始化工作流运行时，释放创建的任务与队列资源 */
esp_err_t wf_runtime_deinit(wf_runtime_t *rt);
/* 执行一轮完整工作流（按 steps 顺序执行） */
esp_err_t wf_runtime_run_once(wf_runtime_t *rt);

/* ---------- message APIs ---------- */
/* 发送消息到目标任务邮箱（阻塞时长由 ticks_to_wait 控制） */
esp_err_t wf_send(wf_runtime_t *rt, uint8_t from_task_id, uint8_t to_task_id,
                  uint16_t msg_id, uint32_t value, void *ptr, TickType_t ticks_to_wait);
/* 从当前任务邮箱接收一条消息（超时返回 ESP_ERR_TIMEOUT） */
esp_err_t wf_recv(wf_runtime_t *rt, uint8_t self_task_id, wf_message_t *out_msg, TickType_t ticks_to_wait);

#ifdef __cplusplus
}
#endif

#endif /* WORKFLOW_H */
