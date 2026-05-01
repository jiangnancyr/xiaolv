// main/utils/logger.c
#include "logger.h"
#include <stdarg.h>
#include <time.h>
#include "esp_log.h"
#include "esp_system.h"

static log_level_t g_log_level = LOG_LEVEL_INFO;

static const char *level_str[] = {
    "",
    "E",
    "W",
    "I",
    "D",
    "V"
};

void logger_init(void)
{
    // 初始化ESP日志
    esp_log_level_set("*", ESP_LOG_INFO);
    LOG_I("LOGGER", "Logger system initialized");
}

void logger_set_level(log_level_t level)
{
    g_log_level = level;
    LOG_I("LOGGER", "Log level set to %d", level);
}

void logger_log(log_level_t level, const char *tag, int line, const char *fmt, ...)
{
    if (level > g_log_level) return;
    
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    // 获取当前任务名称
    char *task_name = pcTaskGetName(NULL);
    
    // // 格式化输出
    printf("[%s] [%s] [%s:%d] %s\n", 
           level_str[level], task_name, tag, line, buffer);
    
    // 同时输出到ESP日志
    esp_log_write(ESP_LOG_INFO, tag, "%s\n", buffer);
}