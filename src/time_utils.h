#pragma once

#include <stdint.h>

#if defined(_WIN32)
#include <windows.h>
static inline uint64_t get_time_ms(void) {
    static LARGE_INTEGER freq;
    static int initialized = 0;
    if (!initialized) {
        QueryPerformanceFrequency(&freq);
        initialized = 1;
    }
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (uint64_t)((now.QuadPart * 1000) / freq.QuadPart);
}
#else
#include <time.h>
static inline uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
#endif

static inline int64_t get_time_ms_signed(void) {
    return (int64_t)get_time_ms();
}
