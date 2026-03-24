#pragma once

#include <pthread.h>
#include <string>
#include <vector>
#include <ctime>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include "mutex_utils.hpp"


class FileQueue {
public:
    FileQueue(const std::vector<std::string>& files)
        : files_(files), index_(0), copied_(0) {
        pthread_mutex_init(&mutex_, nullptr);
    }

    ~FileQueue() {
        pthread_mutex_destroy(&mutex_);
    }

    // Возвращает следующий файл для обработки.
    // Пустая строка — файлов больше нет, поток должен завершиться.
    std::string next_file() {

        int rc = mutex_timedlock(&mutex_, 5);
        if (rc == ETIMEDOUT) {
            fprintf(stderr,
                "Possible deadlock: thread %lu waiting for mutex > 5s\n",
                (unsigned long)pthread_self());
            return "";  // завершаем поток
        }

        std::string result;
        if (index_ < (int)files_.size()) {
            result = files_[index_++];
        }

        pthread_mutex_unlock(&mutex_);
        return result;
    }

    // Вызывается воркером после успешного копирования файла.
    void increment_copied() {
        int rc = mutex_timedlock(&mutex_, 5);

        if (rc == ETIMEDOUT) {
            fprintf(stderr,
                "Possible deadlock: thread %lu waiting for mutex > 5s\n",
                (unsigned long)pthread_self());
            return;
        }

        copied_++;

        pthread_mutex_unlock(&mutex_);
    }

    int get_copied() const { return copied_; }

    pthread_mutex_t* get_mutex() { return &mutex_; }
private:
    std::vector<std::string> files_;  // все входные файлы
    int                      index_;  // индекс следующего незанятого файла
    int                      copied_; // счётчик успешно скопированных
    pthread_mutex_t          mutex_;  // единственный мьютекс (условие задания)
};
