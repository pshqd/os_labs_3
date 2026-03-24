#pragma once

#include <pthread.h>
#include <cerrno>
#include <time.h>

// pthread_mutex_timedlock недоступен на macOS — эмулируем через trylock
static int mutex_timedlock(pthread_mutex_t* mutex, int timeout_sec) {
    struct timespec wait = {0, 10000000};  // 10 мс между попытками
    int total_ms = 0;
    const int limit_ms = timeout_sec * 1000;

    while (total_ms < limit_ms) {
        if (pthread_mutex_trylock(mutex) == 0) return 0;
        nanosleep(&wait, nullptr);
        total_ms += 10;
    }
    return ETIMEDOUT;
}
