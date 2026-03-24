#include <iostream>
#include <vector>
#include <string>
#include <pthread.h>
#include "file_queue.hpp"
#include "logger.hpp"
#include "worker.hpp"

static const int NUM_WORKERS = 3;

int main(int argc, char* argv[]) {
    // ./secure_copy file1.txt file2.txt ... outdir/ key
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0]
                  << " <file1> [file2 ...] <output_dir> <key>\n";
        return 1;
    }

    // Последний аргумент — ключ, предпоследний — выходная директория,
    // всё остальное — входные файлы
    char        key        = argv[argc - 1][0];
    const char* output_dir = argv[argc - 2];

    std::vector<std::string> files;
    for (int i = 1; i <= argc - 3; i++) {
        files.push_back(argv[i]);
    }

    if (files.empty()) {
        std::cerr << "Error: no input files provided\n";
        return 1;
    }

    // Создаём общие объекты — один на всю программу
    FileQueue queue(files);
    Logger    logger;

    // Один WorkerArgs на каждый поток — данные разные, queue/logger общие
    WorkerArgs args[NUM_WORKERS];
    for (int i = 0; i < NUM_WORKERS; i++) {
        args[i].queue      = &queue;
        args[i].logger     = &logger;
        args[i].output_dir = output_dir;
        args[i].key        = key;
    }

    // Запускаем 3 потока
    pthread_t threads[NUM_WORKERS];
    for (int i = 0; i < NUM_WORKERS; i++) {
        if (pthread_create(&threads[i], nullptr, worker_thread, &args[i]) != 0) {
            std::cerr << "Error: failed to create thread " << i << "\n";
            return 1;
        }
        std::cout << "Started worker thread " << i
                  << " (tid=" << threads[i] << ")\n";
    }

    // Ждём завершения всех потоков
    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_join(threads[i], nullptr);
    }

    std::cout << "Done. Files copied: " << queue.get_copied() << "\n";
    std::cout << "Log written to: log.txt\n";
    return 0;
}
