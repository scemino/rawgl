#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*game_debug_func_t)(void* user_data, uint64_t pins);
typedef struct {
    struct {
        game_debug_func_t func;
        void* user_data;
    } callback;
    bool* stopped;
} game_debug_t;

#ifdef __cplusplus
} /* extern "C" */
#endif