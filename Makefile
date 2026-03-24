CXX      = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -pthread -I./deps -I./src

UNAME := $(shell uname)
ifeq ($(UNAME), Darwin)
    RPATH = -Wl,-rpath,@loader_path/deps
else
    RPATH = -Wl,-rpath,'$$ORIGIN/deps'
endif

LDFLAGS = -L./deps -lcaesar -pthread $(RPATH)

TARGET = secure_copy
SRCS   = src/main.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
	rm -f log.txt
	rm -rf outdir/

# Создаём 5 тестовых файлов и запускаем программу
test: all
	mkdir -p test_inputs
	echo "File one content"   > test_inputs/file1.txt
	echo "File two content"   > test_inputs/file2.txt
	echo "File three content" > test_inputs/file3.txt
	echo "File four content"  > test_inputs/file4.txt
	echo "File five content"  > test_inputs/file5.txt
	./$(TARGET) test_inputs/file1.txt test_inputs/file2.txt \
	            test_inputs/file3.txt test_inputs/file4.txt \
	            test_inputs/file5.txt outdir/ K
	@echo ""
	@echo "=== Encrypted files in outdir/ ==="
	@ls -la outdir/
	@echo ""
	@echo "=== log.txt ==="
	@cat log.txt

# Проверяем шифрование: зашифровать и расшифровать = оригинал
test_roundtrip: all
	mkdir -p test_inputs roundtrip_out
	echo "Hello roundtrip" > test_inputs/roundtrip.txt
	./$(TARGET) test_inputs/roundtrip.txt outdir/ K
	./$(TARGET) outdir/roundtrip.txt roundtrip_out/ K
	diff test_inputs/roundtrip.txt roundtrip_out/roundtrip.txt \
		&& echo "ROUNDTRIP PASSED" || echo "ROUNDTRIP FAILED"

.PHONY: all clean test test_roundtrip
