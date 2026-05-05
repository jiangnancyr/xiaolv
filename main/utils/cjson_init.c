#include "cjson_init.h"
#include "esp_heap_caps.h"

// 自定义 PSRAM 分配函数
void* cjson_spiram_malloc(size_t size) 
{
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
}

// 自定义释放函数（使用普通 free 即可）
void cjson_spiram_free(void* ptr) 
{
    free(ptr);
}

// 初始化 cJSON 使用 PSRAM
void cjson_use_psram(void) 
{
    cJSON_Hooks hooks = {
        .malloc_fn = cjson_spiram_malloc,
        .free_fn = cjson_spiram_free
    };
    cJSON_InitHooks(&hooks);
}

// 在程序初始化时调用
void cjson_setup(void) 
{
    cjson_use_psram();
    // 后续所有 cJSON_CreateXXX 都会使用 PSRAM
}