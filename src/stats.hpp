#pragma once

#include <string>
#include <vector>
#include <cstdio>

//  Статистика обработки одного файла

struct FileStats {
    std::string filename;   // имя файла (без пути)
    double      duration;   // время обработки, секунды
    bool        success;    // скопирован/зашифрован успешно?
};

//  Режимы работы программы

enum class Mode {
    SEQUENTIAL,
    PARALLEL,
    AUTO
};

// Строковое имя режима — для вывода в таблицу
inline const char* mode_name(Mode m) {
    switch (m) {
        case Mode::SEQUENTIAL: return "sequential";
        case Mode::PARALLEL:   return "parallel";
        case Mode::AUTO:       return "auto";
    }
    return "unknown";
}

//  Итоговая статистика одного прогона

struct RunStats {
    Mode                   mode;
    std::vector<FileStats> files;       // результат по каждому файлу
    double                 total_time;  // реальное wall-clock время всего прогона

    // Среднее время на один файл
    double avg_time() const {
        if (files.empty()) return 0.0;
        double sum = 0.0;
        for (const auto& f : files) sum += f.duration;
        return sum / (double)files.size();
    }

    // Сколько файлов обработано успешно
    int success_count() const {
        int n = 0;
        for (const auto& f : files) if (f.success) n++;
        return n;
    }
};


//  Вывод статистики одного прогона

inline void print_stats(const RunStats& s) {
    printf("\n┌─── Statistics: %s mode ──────────────────────────┐\n",
           mode_name(s.mode));
    printf("│  Files processed : %d / %d\n",
           s.success_count(), (int)s.files.size());
    printf("│  Total time      : %.4f s\n", s.total_time);
    printf("│  Avg time/file   : %.4f s\n", s.avg_time());
    printf("│\n");
    printf("│  %-30s  %10s  %s\n", "File", "Time (s)", "Status");
    printf("│  %-30s  %10s  %s\n",
           "──────────────────────────────", "──────────", "──────");
    for (const auto& f : s.files) {
        printf("│  %-30s  %10.4f  %s\n",
               f.filename.c_str(), f.duration,
               f.success ? "OK" : "FAIL");
    }
    printf("└──────────────────────────────────────────────────┘\n");
}

//  Сравнительная таблица — только для AUTO режима

inline void print_comparison(const RunStats& chosen,
                              const RunStats& alternative) {
    printf("\n╔══════════════════════════════════════════════════╗\n");
    printf("║         AUTO MODE — COMPARISON TABLE            ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║  %-12s │ %10s │ %10s │ %6s ║\n",
           "Mode", "Total (s)", "Avg (s)", "Files");
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║  %-12s │ %10.4f │ %10.4f │ %6d ║\n",
           mode_name(chosen.mode),
           chosen.total_time, chosen.avg_time(), chosen.success_count());
    printf("║  %-12s │ %10.4f │ %10.4f │ %6d ║\n",
           mode_name(alternative.mode),
           alternative.total_time, alternative.avg_time(),
           alternative.success_count());
    printf("╠══════════════════════════════════════════════════╣\n");

    // Вычисляем speedup — во сколько раз выбранный режим быстрее
    double speedup = (alternative.total_time > 0.0)
                     ? alternative.total_time / chosen.total_time
                     : 1.0;

    printf("║  Selected : %-10s  (%.2fx faster than %s)\n",
           mode_name(chosen.mode), speedup, mode_name(alternative.mode));
    printf("╚══════════════════════════════════════════════════╝\n");
}