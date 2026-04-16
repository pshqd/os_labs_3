#pragma once

#include <pthread.h>
#include <cstdio>
#include <ctime>
#include <string>
#include <sys/stat.h>   // mkdir
#include "file_queue.hpp"
#include "logger.hpp"

// Объявление функций из libcaesar.so
extern "C" {
    void set_key(char key);
    void caesar(const char* src, char* dst, int len);
}

// Всё что нужно воркеру — кладём в одну структуру
struct WorkerArgs {
    FileQueue*  queue;
    Logger*     logger;
    const char* output_dir;
    char        key;
};

void* worker_thread(void* arg) {
    auto* args = static_cast<WorkerArgs*>(arg);

    // Создаём выходную директорию если не существует
    mkdir(args->output_dir, 0755);
    
    while (true) {
        // 1. Берём следующий файл из очереди
        std::string filepath = args->queue->next_file();
        if (filepath.empty()) break;  // файлов больше нет — завершаемся

        // Засекаем время начала обработки
        struct timespec t_start, t_end;
        clock_gettime(CLOCK_MONOTONIC, &t_start);

        // Извлекаем только имя файла (без пути) для выходного файла
        // например: "inputs/hello.txt" → "hello.txt"
        std::string filename = filepath;
        size_t slash = filepath.rfind('/');
        if (slash != std::string::npos)
            filename = filepath.substr(slash + 1);

        std::string outpath = std::string(args->output_dir) + "/" + filename;

        // 2. Читаем входной файл целиком
        FILE* fin = fopen(filepath.c_str(), "rb");
        if (!fin) {
            fprintf(stderr, "worker %lu: cannot open %s\n",
                (unsigned long)pthread_self(), filepath.c_str());
            args->logger->log(args->queue->get_mutex(),
                filepath, /*success=*/false, 0.0);
            continue;  // берём следующий файл
        }

        // Узнаём размер файла
        fseek(fin, 0, SEEK_END);
        long file_size = ftell(fin);
        rewind(fin);

        // Читаем содержимое в буфер
        char* buf = new char[file_size];
        fread(buf, 1, file_size, fin);
        fclose(fin);

        // 3. Шифруем через caesar XOR
        
        caesar(buf, buf, (int)file_size);  // src == dst — допустимо

        // 4. Пишем в выходной файл
        FILE* fout = fopen(outpath.c_str(), "wb");
        bool success = false;
        if (fout) {
            fwrite(buf, 1, file_size, fout);
            fclose(fout);
            success = true;
        } else {
            fprintf(stderr, "worker %lu: cannot write %s\n",
                (unsigned long)pthread_self(), outpath.c_str());
        }

        delete[] buf;

        // 5. Инкрементируем счётчик скопированных файлов
        if (success) args->queue->increment_copied();

        // 6. Логируем результат
        clock_gettime(CLOCK_MONOTONIC, &t_end);
        double elapsed = (t_end.tv_sec - t_start.tv_sec)
                       + (t_end.tv_nsec - t_start.tv_nsec) / 1e9;

        args->logger->log(args->queue->get_mutex(), filepath, success, elapsed);
    }

    return nullptr;
}
