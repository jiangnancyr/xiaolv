// main/utils/logger.h
#ifndef LOGGER_H
#define LOGGER_H

#include "config.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef enum {
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_VERBOSE
} log_level_t;

// 日志宏定义
#define LOG_E(tag, fmt, ...)   logger_log(LOG_LEVEL_ERROR, tag, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_W(tag, fmt, ...)   logger_log(LOG_LEVEL_WARN, tag, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_I(tag, fmt, ...)   logger_log(LOG_LEVEL_INFO, tag, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_D(tag, fmt, ...)   logger_log(LOG_LEVEL_DEBUG, tag, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_V(tag, fmt, ...)   logger_log(LOG_LEVEL_VERBOSE, tag, __LINE__, fmt, ##__VA_ARGS__)

// 函数声明
void logger_init(void);
void logger_set_level(log_level_t level);
void logger_log(log_level_t level, const char *tag, int line, const char *fmt, ...);

#endif // LOGGER_H