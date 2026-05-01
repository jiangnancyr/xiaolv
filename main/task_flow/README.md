# Workflow 框架使用说明

该框架用于 ESP32 + FreeRTOS 场景，支持：

- 任务按步骤串行/并行执行
- 通过宏定义组装任务和流程
- 任务间通过 mailbox 发消息

## 1. 定义任务 ID

```c
typedef enum {
    TASK_SENSOR = 1,
    TASK_FILTER = 2,
    TASK_UPLOAD = 3,
} task_id_t;
```

## 2. 定义任务函数

```c
static esp_err_t sensor_task(wf_runtime_t *rt, uint8_t self_id, void *arg)
{
    (void)arg;
    // 给 filter 发一条消息
    return wf_send(rt, self_id, TASK_FILTER, 100, 1234, NULL, pdMS_TO_TICKS(10));
}

static esp_err_t filter_task(wf_runtime_t *rt, uint8_t self_id, void *arg)
{
    (void)arg;
    wf_message_t msg = {0};
    if (wf_recv(rt, self_id, &msg, pdMS_TO_TICKS(50)) == ESP_OK) {
        return wf_send(rt, self_id, TASK_UPLOAD, 200, msg.value + 1, NULL, pdMS_TO_TICKS(10));
    }
    return ESP_OK;
}

static esp_err_t upload_task(wf_runtime_t *rt, uint8_t self_id, void *arg)
{
    (void)arg;
    wf_message_t msg = {0};
    if (wf_recv(rt, self_id, &msg, pdMS_TO_TICKS(50)) == ESP_OK) {
        // 执行上报
    }
    return ESP_OK;
}
```

## 3. 宏组装任务和流程

```c
static const wf_task_desc_t g_tasks[] = {
    WF_TASK(TASK_SENSOR, "sensor", sensor_task, NULL, 4096, 5, -1),
    WF_TASK(TASK_FILTER, "filter", filter_task, NULL, 4096, 5, -1),
    WF_TASK(TASK_UPLOAD, "upload", upload_task, NULL, 4096, 5, -1),
};

static const wf_step_desc_t g_steps[] = {
    // 第一步：并行执行 sensor/filter
    WF_STEP_PARALLEL_N("collect+filter", TASK_SENSOR, TASK_FILTER),
    // 第二步：串行执行 upload
    WF_STEP_SERIAL_N("upload", TASK_UPLOAD),
};

static const wf_def_t g_flow = WF_DEF("demo_flow", g_tasks, g_steps);
```

## 4. 运行

```c
wf_runtime_t rt;
ESP_ERROR_CHECK(wf_runtime_init(&rt, &g_flow));

for (;;) {
    ESP_ERROR_CHECK(wf_runtime_run_once(&rt));
    vTaskDelay(pdMS_TO_TICKS(1000));
}
```

## 5. 使用 task_manager 动态组装流程

`task_manager` 是对 `workflow` 的一层封装，用于在运行时按顺序添加任务、添加步骤、构建并启动流程。适合任务列表或步骤需要由代码动态组装的场景。

### 5.1 引入头文件

```c
#include "task_manager.h"
```

### 5.2 基本使用流程

```c
static task_manager_t g_task_manager;

static esp_err_t start_demo_flow(void)
{
    ESP_RETURN_ON_ERROR(task_manager_init(&g_task_manager, "demo_flow"), TAG, "task manager init failed");

    ESP_RETURN_ON_ERROR(task_manager_add_task(&g_task_manager, TASK_SENSOR, &(tm_task_config_t){
        .name = "sensor",
        .fn = sensor_task,
        .arg = NULL,
        .stack_size = 4096,
        .priority = 5,
        .core_id = -1,
    }), TAG, "add sensor task failed");

    ESP_RETURN_ON_ERROR(task_manager_add_task(&g_task_manager, TASK_FILTER, &(tm_task_config_t){
        .name = "filter",
        .fn = filter_task,
        .arg = NULL,
        .stack_size = 4096,
        .priority = 5,
        .core_id = -1,
    }), TAG, "add filter task failed");

    ESP_RETURN_ON_ERROR(task_manager_add_task(&g_task_manager, TASK_UPLOAD, &(tm_task_config_t){
        .name = "upload",
        .fn = upload_task,
        .arg = NULL,
        .stack_size = 4096,
        .priority = 5,
        .core_id = -1,
    }), TAG, "add upload task failed");

    const uint8_t collect_tasks[] = {TASK_SENSOR, TASK_FILTER};
    ESP_RETURN_ON_ERROR(task_manager_add_step(&g_task_manager,
                                              "collect+filter",
                                              TM_STEP_PARALLEL,
                                              collect_tasks,
                                              sizeof(collect_tasks) / sizeof(collect_tasks[0])),
                        TAG, "add collect step failed");

    const uint8_t upload_tasks[] = {TASK_UPLOAD};
    ESP_RETURN_ON_ERROR(task_manager_add_step(&g_task_manager,
                                              "upload",
                                              TM_STEP_SERIAL,
                                              upload_tasks,
                                              sizeof(upload_tasks) / sizeof(upload_tasks[0])),
                        TAG, "add upload step failed");

    ESP_RETURN_ON_ERROR(task_manager_start(&g_task_manager), TAG, "task manager start failed");
    return task_manager_run_once(&g_task_manager);
}
```

### 5.3 生命周期

```c
ESP_ERROR_CHECK(task_manager_start(&g_task_manager));
ESP_ERROR_CHECK(task_manager_run_once(&g_task_manager));
ESP_ERROR_CHECK(task_manager_stop(&g_task_manager));
ESP_ERROR_CHECK(task_manager_deinit(&g_task_manager));
```

### 5.4 接口说明

- `task_manager_init()`：初始化管理器并设置流程名称。
- `task_manager_add_task()`：添加任务描述，必须在 `task_manager_start()` 前调用。
- `task_manager_add_step()`：添加串行或并行步骤，步骤中的任务 ID 必须已经通过 `task_manager_add_task()` 注册。
- `task_manager_build()`：仅组装出 `wf_def_t`，不启动 worker；需要直接使用底层 `workflow` 时可调用。
- `task_manager_start()`：构建流程并初始化 `wf_runtime_t`。
- `task_manager_run_once()`：执行一轮已启动的流程。
- `task_manager_stop()`：释放 `workflow` runtime 创建的 worker 和队列。
- `task_manager_deinit()`：停止流程并清空管理器状态。

### 5.5 注意事项

- 最大任务数由 `TM_MAX_TASKS` 控制，默认等于 `WF_MAX_TASKS`。
- 最大步骤数由 `TM_MAX_STEPS` 控制，默认是 `8`。
- 已启动后不能继续添加任务或步骤；如需重新组装，先调用 `task_manager_stop()` 或 `task_manager_deinit()`。
- `core_id` 小于 `0` 表示不绑定 CPU core。
- `stack_size` 或 `priority` 填 `0` 时会使用默认值。
