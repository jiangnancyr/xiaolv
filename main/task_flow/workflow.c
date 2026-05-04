#include "workflow.h"
#include <string.h>
#include "logger.h"

#define TAG "WORKFLOW"

/* worker 上下文：把运行时对象和任务索引传递给通用 worker 入口函数 */
typedef struct {
    wf_runtime_t *rt;
    uint8_t index;
} wf_worker_ctx_t;

/* 静态 worker 上下文数组：一项对应一个 workflow 任务 */
static wf_worker_ctx_t s_worker_ctx[WF_MAX_TASKS];

/* 通过任务ID查找其在任务表中的索引，找不到返回 -1 */
static int wf_index_by_task_id(const wf_runtime_t *rt, uint8_t task_id)
{
    if (!rt || !rt->def) {
        return -1;
    }
    for (int i = 0; i < rt->def->task_count; i++) {
        if (rt->def->tasks[i].id == task_id) {
            return i;
        }
    }
    return -1;
}

/* worker 主循环：等待调度通知，执行一次任务函数，然后上报完成事件 */
static void wf_worker_entry(void *param)
{
    wf_worker_ctx_t *ctx = (wf_worker_ctx_t *)param;
    wf_runtime_t *rt = ctx->rt;
    const wf_task_desc_t *task_desc = &rt->def->tasks[ctx->index];
    uint8_t done_index = ctx->index;
    wf_task_done_msg_t done_msg = {
        .task_id = task_desc->id,
        .result = ESP_OK,
    };
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        esp_err_t err = task_desc->fn(rt, task_desc->id, task_desc->arg);
        if (err != ESP_OK) {
            done_msg.result = err;
            LOG_W(TAG, "Task %s run failed: %s", task_desc->name, esp_err_to_name(err));
        }

        if (xQueueSend(rt->done_queue, &done_msg, portMAX_DELAY) != pdPASS) {
            LOG_E(TAG, "Done queue full, task=%s", task_desc->name);
        }
    }
}

/* 执行单个步骤：根据步骤类型进行串行或并行调度，并等待任务完成 */
static esp_err_t wf_run_step(wf_runtime_t *rt, const wf_step_desc_t *step)
{
    if (!rt || !step || step->task_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t expected_done = 0;
    for (int i = 0; i < step->task_count; i++) {
        int idx = wf_index_by_task_id(rt, step->task_ids[i]);
        if (idx < 0) {
            LOG_E(TAG, "Unknown task id %u in step %s", step->task_ids[i], step->name ? step->name : "unnamed");
            return ESP_ERR_NOT_FOUND;
        }

        xTaskNotifyGive(rt->worker_handles[idx]);
        expected_done++;

        if (step->type == WF_STEP_SERIAL) {
            wf_task_done_msg_t done_msg;
            if (xQueueReceive(rt->done_queue, &done_msg, portMAX_DELAY) != pdPASS) {
                return ESP_FAIL;
            }
            // 当出现一个任务执行失败就立即返回错误，不继续执行后续任务
            if (done_msg.result != ESP_OK) {
                LOG_W(TAG, "Task %u in step %s failed: %s", done_msg.task_id, step->name ? step->name : "unnamed", esp_err_to_name(done_msg.result));
                return done_msg.result;
            }
            expected_done--;
        }
    }

    while (expected_done > 0) {
        wf_task_done_msg_t done_msg;
        if (xQueueReceive(rt->done_queue, &done_msg, portMAX_DELAY) != pdPASS) {
            return ESP_FAIL;
        }
        if (done_msg.result != ESP_OK) {
            LOG_W(TAG, "Task %u in step %s failed: %s", done_msg.task_id, step->name ? step->name : "unnamed", esp_err_to_name(done_msg.result));
            return done_msg.result;
        }
        expected_done--;
    }

    return ESP_OK;
}

/* 初始化运行时：创建完成队列、每个任务的邮箱以及对应 worker 任务 */
esp_err_t wf_runtime_init(wf_runtime_t *rt, const wf_def_t *def)
{
    if (!rt || !def || !def->tasks || !def->steps) {
        return ESP_ERR_INVALID_ARG;
    }
    if (def->task_count == 0 || def->task_count > WF_MAX_TASKS) {
        return ESP_ERR_INVALID_SIZE;
    }

    memset(rt, 0, sizeof(*rt));
    rt->def = def;
    rt->runner_task = xTaskGetCurrentTaskHandle();
    rt->done_queue = xQueueCreate(def->task_count, sizeof(wf_task_done_msg_t));
    if (!rt->done_queue) {
        LOG_E(TAG, "Create done queue failed");
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < def->task_count; i++) {
        rt->mailbox[i] = xQueueCreate(WF_MAILBOX_LEN, sizeof(wf_message_t));
        if (!rt->mailbox[i]) {
            LOG_E(TAG, "Create mailbox failed: %s", def->tasks[i].name);
            wf_runtime_deinit(rt);
            return ESP_ERR_NO_MEM;
        }

        s_worker_ctx[i].rt = rt;
        s_worker_ctx[i].index = i;
        BaseType_t ok = pdFAIL;

        if (def->tasks[i].core_id >= 0) {
            ok = xTaskCreatePinnedToCore(
                wf_worker_entry,
                def->tasks[i].name,
                def->tasks[i].stack_size,
                &s_worker_ctx[i],
                def->tasks[i].priority,
                &rt->worker_handles[i],
                def->tasks[i].core_id 
            );
        } else {
            ok = xTaskCreate(
                wf_worker_entry,
                def->tasks[i].name,
                def->tasks[i].stack_size,
                &s_worker_ctx[i],
                def->tasks[i].priority,
                &rt->worker_handles[i]
            );
        }

        if (ok != pdPASS) {
            LOG_E(TAG, "Create worker failed: %s", def->tasks[i].name);
            wf_runtime_deinit(rt);
            return ESP_ERR_NO_MEM;
        }
    }

    LOG_I(TAG, "Workflow %s initialized (tasks=%u, steps=%u)",
          def->name ? def->name : "unnamed", def->task_count, def->step_count);
    return ESP_OK;
}

/* 释放运行时资源：删除 worker、邮箱和完成队列 */
esp_err_t wf_runtime_deinit(wf_runtime_t *rt)
{
    if (!rt) {
        return ESP_ERR_INVALID_ARG;
    }

    if (rt->def) {
        for (int i = 0; i < rt->def->task_count; i++) {
            if (rt->worker_handles[i]) {
                vTaskDelete(rt->worker_handles[i]);
                rt->worker_handles[i] = NULL;
            }
            if (rt->mailbox[i]) {
                vQueueDelete(rt->mailbox[i]);
                rt->mailbox[i] = NULL;
            }
        }
    }

    if (rt->done_queue) {
        vQueueDelete(rt->done_queue);
        rt->done_queue = NULL;
    }

    rt->def = NULL;
    rt->runner_task = NULL;
    return ESP_OK;
}

/* 执行一轮流程：按定义顺序逐个执行所有步骤 */
esp_err_t wf_runtime_run_once(wf_runtime_t *rt)
{
    if (!rt || !rt->def) {
        return ESP_ERR_INVALID_STATE;
    }

    for (int i = 0; i < rt->def->step_count; i++) {
        const wf_step_desc_t *step = &rt->def->steps[i];
        esp_err_t err = wf_run_step(rt, step);
        if (err != ESP_OK) {
            LOG_E(TAG, "Run step[%d] %s failed: %s",
                  i, step->name ? step->name : "unnamed", esp_err_to_name(err));
            return err;
        } else {
            LOG_I(TAG, "Run step[%d]", i);
        }
    }
    return ESP_OK;
}

/* 发送任务消息：根据目标任务ID定位邮箱并投递 */
esp_err_t wf_send(wf_runtime_t *rt, uint8_t from_task_id, uint8_t to_task_id,
                  uint16_t msg_id, uint32_t value, void *ptr, TickType_t ticks_to_wait)
{
    if (!rt || !rt->def) {
        return ESP_ERR_INVALID_STATE;
    }

    int to_idx = wf_index_by_task_id(rt, to_task_id);
    if (to_idx < 0) {
        return ESP_ERR_NOT_FOUND;
    }

    wf_message_t msg = {
        .msg_id = msg_id,
        .from_task = from_task_id,
        .to_task = to_task_id,   
        .value = value,
        .ptr = ptr,
    };

    return (xQueueSend(rt->mailbox[to_idx], &msg, ticks_to_wait) == pdPASS) ? ESP_OK : ESP_ERR_TIMEOUT;
}

/* 接收任务消息：从当前任务对应邮箱读取一条消息 */
esp_err_t wf_recv(wf_runtime_t *rt, uint8_t self_task_id, wf_message_t *out_msg, TickType_t ticks_to_wait)
{
    if (!rt || !rt->def || !out_msg) {
        return ESP_ERR_INVALID_ARG;
    }

    int self_idx = wf_index_by_task_id(rt, self_task_id);
    if (self_idx < 0) {
        return ESP_ERR_NOT_FOUND;
    }

    return (xQueueReceive(rt->mailbox[self_idx], out_msg, ticks_to_wait) == pdPASS) ? ESP_OK : ESP_ERR_TIMEOUT;
}
