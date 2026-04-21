# OS Labs — Многопоточное шифрование файлов

Цикл из трёх практических работ по системному программированию на C++.
Каждая работа расширяет предыдущую: от простой библиотеки — к многопоточному
копировщику с синхронизацией.

---

## Работа 1 — Библиотека шифрования libcaesar

**Цель:** создать динамическую библиотеку `libcaesar.so` с XOR-шифрованием.

### Интерфейс

```c++
void set_key(char key);                          // установить ключ шифрования
void caesar(const char* src, char* dst, int n);  // зашифровать n байт
```

XOR-шифрование обратимо: повторное применение с тем же ключом расшифровывает данные.

```text
"Hello" XOR 'K' → зашифровано → XOR 'K' → "Hello"
```

### Сборка

```bash
make          # собрать libcaesar.so
make install  # скопировать в deps/
make test     # запустить тесты через test_caesar.py
```

### Файлы

```text
caesar.cpp        — реализация set_key() и caesar()
caesar.h          — заголовочный файл библиотеки
Makefile          — сборка .so, установка, тесты
test_caesar.py    — автотесты на Python
```

---

## Работа 2 — Файловый копировщик с шифрованием

**Цель:** на основе `libcaesar.so` написать `secure_copy` —
программу которая копирует файл с шифрованием через два потока.

### Архитектура: Producer / Consumer

```text
Producer поток              Consumer поток
──────────────              ──────────────
читает файл блоками    →    шифрует блоки
кладёт в BoundedBuffer →    пишет в файл
```

`BoundedBuffer` — кольцевая очередь блоков с синхронизацией
через `pthread_mutex_t` + `pthread_cond_t`.
Если буфер полон — producer ждёт. Если пуст — consumer ждёт.

При нажатии Ctrl+C флаг `keep_running = 0` останавливает producer.
Consumer дочитывает оставшееся из буфера и завершается штатно.

### Запуск

```bash
./secure_copy input.txt output.txt K
```

### Файлы

```text
src/main.cpp           — точка входа, SIGINT, запуск потоков
src/bounded_buffer.hpp — потокобезопасный кольцевой буфер
src/producer.hpp       — поток чтения файла
src/consumer.hpp       — поток шифрования и записи
deps/libcaesar.so      — библиотека из Работы 1
```

---

## Работа 3 — Защита от конфликтов при одновременном копировании

**Цель:** расширить `secure_copy` для обработки N файлов параллельно
через три рабочих потока с мьютексной синхронизацией и логированием.

### Архитектура: Worker Pool

```text
[file1, file2, file3, file4, file5]  ← общая очередь
          ↓         ↓         ↓
       worker-0  worker-1  worker-2
       читает    читает    читает
       шифрует   шифрует   шифрует
       пишет     пишет     пишет
```

Воркеры берут файлы из очереди динамически — кто освободился, тот и берёт следующий.

### Запуск

```bash
./secure_copy file1.txt file2.txt file3.txt outdir/ K
make test     # запуск на 5 файлах автоматически
```

### Синхронизация

Единственный `pthread_mutex_t` защищает три критические секции:

| Операция | Зачем мьютекс |
|---|---|
| `next_file()` | два воркера не должны получить один файл |
| `increment_copied()` | `copied_++` не атомарна без защиты |
| `log()` | два потока не пишут в файл одновременно |

Мьютекс удерживается только на короткие операции.
Тяжёлое (чтение, шифрование, запись) — вне мьютекса, параллельно.

### Обнаружение дедлоков

```cpp
int rc = mutex_timedlock(&mutex_, 5);
if (rc == ETIMEDOUT) {
    fprintf(stderr, "Possible deadlock: thread %lu waiting > 5s\n", ...);
    return "";
}
```

На macOS `pthread_mutex_timedlock` недоступен — эмулируется через
`pthread_mutex_trylock` + `nanosleep` в `mutex_utils.hpp`.

### Логирование

```text
[2026-04-16 10:40:17] thread=6157021184 file=test_inputs/file1.txt status=OK time=0.001s
```

Файл `log.txt` открывается в режиме `append` — повторные запуски
дописывают строки, не удаляя предыдущие.

### Файлы

```text
src/main.cpp           — точка входа, 3 потока, pthread_join
src/worker.hpp         — полный цикл воркера: читать→шифровать→писать
src/file_queue.hpp     — потокобезопасная очередь + счётчик
src/logger.hpp         — запись в log.txt с временной меткой
src/mutex_utils.hpp    — эмуляция timedlock для macOS
deps/libcaesar.so      — библиотека из Работы 1
```

---

## Эволюция архитектуры

```text
Работа 1          Работа 2              Работа 3
────────          ────────              ────────
libcaesar.so  →   Producer            [очередь файлов]
set_key()         BoundedBuffer   →      worker-0
caesar()          Consumer               worker-1
                  1 файл                 worker-2
                  2 потока               мьютекс
                                         log.txt
                                         N файлов
                                         3 потока
```

## Сборка

```bash
make          # собрать secure_copy
make test     # создать 5 файлов и запустить
make clean    # удалить бинарник, log.txt, outdir/
```

## Зависимости

- `g++` с поддержкой C++17
- `libcaesar.so` (собирается из `caesar.cpp`)
- POSIX threads (`-pthread`)
