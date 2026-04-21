#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <ctime>
#include <pthread.h>
#include "file_queue.hpp"
#include "logger.hpp"
#include "worker.hpp"
#include "stats.hpp"
#include "sequential.hpp"


static const int NUM_WORKERS = 4;


// ── Параллельный режим вынесен в функцию (симметрично run_sequential) ──
static RunStats run_parallel(const std::vector<std::string>& files,
                              const char* output_dir, char key) {
    RunStats stats;
    stats.mode = Mode::PARALLEL;

    FileQueue              queue(files);
    Logger                 logger;
    std::vector<FileStats> stats_vec;
    pthread_mutex_t        stats_mutex;
    pthread_mutex_init(&stats_mutex, nullptr);

    WorkerArgs args[NUM_WORKERS];
    for (int i = 0; i < NUM_WORKERS; i++) {
        args[i].queue       = &queue;
        args[i].logger      = &logger;
        args[i].output_dir  = output_dir;
        args[i].key         = key;
        args[i].stats_vec   = &stats_vec;
        args[i].stats_mutex = &stats_mutex;
    }
    set_key(key);

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    pthread_t threads[NUM_WORKERS];
    for (int i = 0; i < NUM_WORKERS; i++) {
        if (pthread_create(&threads[i], nullptr, worker_thread, &args[i]) != 0) {
            std::cerr << "Error: failed to create thread " << i << "\n";
        }
        std::cout << "Started worker thread " << i
                  << " (tid=" << threads[i] << ")\n";
    }

    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_join(threads[i], nullptr);
    }

    clock_gettime(CLOCK_MONOTONIC, &t_end);
    stats.total_time = (t_end.tv_sec  - t_start.tv_sec)
                     + (t_end.tv_nsec - t_start.tv_nsec) / 1e9;
    stats.files = stats_vec;

    pthread_mutex_destroy(&stats_mutex);
    return stats;
}


int main(int argc, char* argv[]) {
    // ./secure_copy --mode=<sequential|parallel|auto> file1.txt ... outdir/ key
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0]
                  << " --mode=<sequential|parallel|auto>"
                  << " <file1> [file2 ...] <output_dir> <key>\n";
        return 1;
    }

    Mode mode = Mode::AUTO;
    if (std::strcmp(argv[1], "--mode=sequential") == 0) mode = Mode::SEQUENTIAL;
    else if (std::strcmp(argv[1], "--mode=parallel") == 0) mode = Mode::PARALLEL;

    // Последний аргумент — ключ, предпоследний — выходная директория,
    // всё остальное — входные файлы
    char        key        = argv[argc - 1][0];
    const char* output_dir = argv[argc - 2];

    std::vector<std::string> files;
    for (int i = 2; i <= argc - 3; i++) {
        files.push_back(argv[i]);
    }

    // эвристика AUTO — менее 5 файлов → sequential, иначе → parallel
    bool is_auto = (mode == Mode::AUTO);
    if (is_auto) {
        mode = (files.size() < 5) ? Mode::SEQUENTIAL : Mode::PARALLEL;
        std::cout << "[auto] " << files.size() << " files -> "
                  << mode_name(mode) << " mode selected\n";
    }

    // sequential режим
    if (mode == Mode::SEQUENTIAL) {
        RunStats stats = run_sequential(files, output_dir, key);
        print_stats(stats);
        if (is_auto) {
            std::cout << "\n[auto] running alternative (parallel) for comparison...\n";
            RunStats alt = run_parallel(files, output_dir, key);
            print_comparison(stats, alt);
        }
        return 0;
    }

    // parallel режим
    RunStats stats = run_parallel(files, output_dir, key);
    std::cout << "Done. Files copied: " << stats.success_count() << "\n";
    std::cout << "Log written to: log.txt\n";
    print_stats(stats);
    if (is_auto) {
        std::cout << "\n[auto] running alternative (sequential) for comparison...\n";
        RunStats alt = run_sequential(files, output_dir, key);
        print_comparison(stats, alt);
    }

    return 0;
}