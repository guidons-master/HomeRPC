#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void (*log_info)(const char* tag, const char* format, ...);
    void (*log_error)(const char* tag, const char* format, ...);
    void (*log_warn)(const char* tag, const char* format, ...);
} rpc_log_t;

extern rpc_log_t rpc_log;

#ifdef __cplusplus
}
#endif