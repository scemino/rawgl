#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void clock_init(void);
uint32_t clock_frame_time(void);
uint32_t clock_frame_count_60hz(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
