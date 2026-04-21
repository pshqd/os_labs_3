#pragma once

#include <cstdio>
#include <ctime>
#include <string>
#include <vector>
#include <sys/stat.h>    // mkdir
#include "file_queue.hpp"
#include "stats.hpp"

// Объявление функций из libcaesar.so
extern "C" {
    void set_key(char key);
    void caesar(const char* src, char* dst, int len);
}

// ─────────────────────────────────────────────────────────────
//  Вспомогательная функция: обработка одного файла
//  Возвращает FileStats с результатом
//  Вынесена отдельно, чтобы не дублировать её в worker.hpp
// ─────────────────────────────────────────────────────────────
inline FileStats process_file(const std::string& filepath,
                               const char* output_dir) {
    FileStats result;

    // Извлекаем имя файла без пути: "inputs/hello.txt" → "hello.txt"
    result.filename = filepath;
    size_t slash = filepath.rfind('/');
    if (slash != std::string::npos)
        result.filename = filepath.substr(slash + 1);

    result.success  = false;
    result.duration = 0.0;

    std::string outpath = std::string(output_dir) + "/" + result.filename;

    // ── Старт замера времени ──────────────────────────────────
    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    // ── Читаем входной файл ───────────────────────────────────
    FILE* fin = fopen(filepath.c_str(), "rb");
    if (!fin) {
        fprintf(stderr, "sequential: cannot open %s\n", filepath.c_str());
        clock_gettime(CLOCK_MONOTONIC, &t_end);
        result.duration = (t_end.tv_sec  - t_start.tv_sec)
                        + (t_end.tv_nsec - t_start.tv_nsec) / 1e9;
        return result;   // success = false
    }

    fseek(fin, 0, SEEK_END);
    long file_size = ftell(fin);
    rewind(fin);

    char* buf = new char[file_size];
    fread(buf, 1, file_size, fin);
    fclose(fin);
        // ── Шифруем XOR через caesar ──────────────────────────────
    caesar(buf, buf, (int)file_size);   // src == dst допустимо

    // ── Пишем выходной файл ───────────────────────────────────
    FILE* fout = fopen(outpath.c_str(), "wb");
    if (fout) {
        fwrite(buf, 1, file_size, fout);
        fclose(fout);
        result.success = true;
    } else {
        fprintf(stderr, "sequential: cannot write %s\n", outpath.c_str());
    }

    delete[] buf;

    // ── Конец замера времени ──────────────────────────────────
    clock_gettime(CLOCK_MONOTONIC, &t_end);
    result.duration = (t_end.tv_sec  - t_start.tv_sec)
                    + (t_end.tv_nsec - t_start.tv_nsec) / 1e9;

    return result;
}

// ─────────────────────────────────────────────────────────────
//  Главная функция sequential режима
//  Принимает список файлов, возвращает RunStats
// ─────────────────────────────────────────────────────────────
inline RunStats run_sequential(const std::vector<std::string>& files,
                                const char* output_dir,
                                char key) {
    RunStats stats;
    stats.mode = Mode::SEQUENTIAL;

    set_key(key);
    mkdir(output_dir, 0755);   // создаём папку если не существует

    // Замеряем ОБЩЕЕ время прогона снаружи цикла
    struct timespec run_start, run_end;
    clock_gettime(CLOCK_MONOTONIC, &run_start);

        FileQueue queue(files);

    while (true) {
        std::string filepath = queue.next_file();
        if (filepath.empty()) break;   // все файлы обработаны

        FileStats fs = process_file(filepath, output_dir);
        stats.files.push_back(fs);

        printf("  [seq] %-30s  %.4f s  %s\n",
               fs.filename.c_str(),
               fs.duration,
               fs.success ? "OK" : "FAIL");
    }

    clock_gettime(CLOCK_MONOTONIC, &run_end);
    stats.total_time = (run_end.tv_sec  - run_start.tv_sec)
                     + (run_end.tv_nsec - run_start.tv_nsec) / 1e9;

    return stats;
}