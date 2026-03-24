#pragma once

#include <pthread.h>
#include <cstdio>
#include <ctime>
#include <string>
#include "mutex_utils.hpp"

// Единственный лог-файл, открытый в режиме append на всё время работы.
// Закрывается в деструкторе.
class Logger {
public:
    Logger() {
        // "a" — append: старые записи не удаляются при повторных запусках
        log_file_ = fopen("log.txt", "a");
        if (!log_file_) {
            fprintf(stderr, "Logger: cannot open log.txt\n");
        }
    }

    ~Logger() {
        if (log_file_) fclose(log_file_);
    }

    // Пишет одну строку в лог.
    // mutex передаётся снаружи — это тот же самый мьютекс из FileQueue,
    // единственный в программе (ограничение задания).
    void log(pthread_mutex_t* mutex,
             const std::string& filename,
             bool success,
             double elapsed_sec)
    {
        if (!log_file_) return;

        // Формируем временную метку: "2026-03-24 15:07:42"
        char timestamp[32];
        time_t now = time(nullptr);
        struct tm* tm_info = localtime(&now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

        // Собираем строку лога до захвата мьютекса — вне критической секции
        char line[512];
        snprintf(line, sizeof(line),
            "[%s] thread=%lu file=%s status=%s time=%.3fs\n",
            timestamp,
            (unsigned long)pthread_self(),
            filename.c_str(),
            success ? "OK" : "ERROR",
            elapsed_sec);

        // Захватываем мьютекс только на момент записи в файл
        int rc = mutex_timedlock(&mutex_, 5);

        if (rc == ETIMEDOUT) {
            fprintf(stderr,
                "Possible deadlock: thread %lu waiting for log mutex > 5s\n",
                (unsigned long)pthread_self());
            return;
        }

        fputs(line, log_file_);
        fflush(log_file_);  // сбрасываем буфер сразу — чтобы лог не терялся при падении

        pthread_mutex_unlock(mutex);
    }

private:
    FILE* log_file_;
};
