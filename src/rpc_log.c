#include "esp_log.h"
#include "rpc_log.h"
#include <stdio.h>
#include <stdarg.h>
#include "HomeRPC.h"

void log_info(const char* tag, const char* format, ...) {
    if (HomeRPC.log_enable && HomeRPC.log_level <= ESP_LOG_INFO) {
        va_list args;
        va_start(args, format);
        printf("%s[HomeRPC::%s]: ", LOG_COLOR_I, tag);
        vprintf(format, args);
        printf("%s\n", LOG_RESET_COLOR);
        va_end(args);
    }
}

void log_error(const char* tag, const char* format, ...) {
    if (HomeRPC.log_enable && HomeRPC.log_level <= ESP_LOG_ERROR) {
        va_list args;
        va_start(args, format);
        printf("%s[HomeRPC::%s]: ", LOG_COLOR_E, tag);
        vprintf(format, args);
        printf("%s\n", LOG_RESET_COLOR);
        va_end(args);
    }
}

void log_warn(const char* tag, const char* format, ...) {
    if (HomeRPC.log_enable && HomeRPC.log_level <= ESP_LOG_WARN) {
        va_list args;
        va_start(args, format);
        printf("%s[HomeRPC::%s]: ", LOG_COLOR_W, tag);
        vprintf(format, args);
        printf("%s\n", LOG_RESET_COLOR);
        va_end(args);
    }
}

rpc_log_t rpc_log = {
    .log_info = log_info,
    .log_error = log_error,
    .log_warn = log_warn,
};