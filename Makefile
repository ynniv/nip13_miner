# Makefile for NIP-13 Standalone Miner
# No external dependencies required

CC = cc
CFLAGS = -Wall -Wextra -O3 -std=c99
TARGET = nip13_miner
PARALLEL_TARGET = nip13_parallel
SOURCE = nip13_standalone.c
PARALLEL_SOURCE = nip13_parallel.c

# Platform-specific optimizations
UNAME := $(shell uname)
ifeq ($(UNAME), Darwin)
    CFLAGS += -DOSX
endif
ifeq ($(UNAME), Linux)
    CFLAGS += -DLINUX
endif

# CPU optimizations
CFLAGS += -march=native -mtune=native

# Parallel version needs pthread
PARALLEL_CFLAGS = $(CFLAGS) -pthread

all: $(TARGET) $(PARALLEL_TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $@ $<

$(PARALLEL_TARGET): $(PARALLEL_SOURCE)
	$(CC) $(PARALLEL_CFLAGS) -o $@ $<

test: $(TARGET)
	@echo "🧪 Creating test event..."
	@echo '{"id":"","pubkey":"32e1827635450ebb3c5a7d12c1f8e7b2b514439ac10a67eef3d9fd9c5c68e245","created_at":1673347337,"kind":1,"tags":[],"content":"Testing NIP-13 proof of work","sig":""}' > test_event.json
	@echo "🔨 Testing with difficulty 12..."
	./$(TARGET) test_event.json 12 10

test-parallel: $(PARALLEL_TARGET)
	@echo "🧪 Creating test event..."
	@echo '{"id":"","pubkey":"32e1827635450ebb3c5a7d12c1f8e7b2b514439ac10a67eef3d9fd9c5c68e245","created_at":1673347337,"kind":1,"tags":[],"content":"Testing NIP-13 proof of work","sig":""}' > test_event.json
	@echo "🔨 Testing parallel miner with difficulty 12..."
	./$(PARALLEL_TARGET) test_event.json 12 10

clean:
	rm -f $(TARGET) $(PARALLEL_TARGET) test_event.json mined_*.json

benchmark: $(TARGET)
	@echo "⚡ Running benchmarks..."
	@echo "Testing difficulty 8 (should be fast):"
	./$(TARGET) test_event.json 8 5
	@echo "\nTesting difficulty 16 (medium):"
	./$(TARGET) test_event.json 16 20

benchmark-parallel: $(PARALLEL_TARGET)
	@echo "⚡ Running parallel benchmarks..."
	@echo "Testing difficulty 8 (should be fast):"
	./$(PARALLEL_TARGET) test_event.json 8 benchmark 5
	@echo "\nTesting difficulty 16 (medium):"
	./$(PARALLEL_TARGET) test_event.json 16 benchmark 10

install: $(TARGET) $(PARALLEL_TARGET)
	@echo "📦 Installing to /usr/local/bin..."
	sudo cp $(TARGET) /usr/local/bin/
	sudo cp $(PARALLEL_TARGET) /usr/local/bin/
	@echo "✅ Installation complete"

uninstall:
	@echo "🗑️  Uninstalling..."
	sudo rm -f /usr/local/bin/$(TARGET)
	sudo rm -f /usr/local/bin/$(PARALLEL_TARGET)
	@echo "✅ Uninstall complete"

help:
	@echo "🎯 NIP-13 Standalone Miner - Available targets:"
	@echo "  all              - Build both miners (single and parallel)"
	@echo "  $(TARGET)     - Build single-threaded miner only"  
	@echo "  $(PARALLEL_TARGET) - Build parallel miner only"
	@echo "  test             - Build and run quick test (single-threaded)"
	@echo "  test-parallel    - Build and run quick test (parallel)"
	@echo "  benchmark        - Run performance benchmarks (single-threaded)"
	@echo "  benchmark-parallel - Run performance benchmarks (parallel)"
	@echo "  clean            - Remove built files"
	@echo "  install          - Install both miners to system PATH"
	@echo "  uninstall        - Remove from system"
	@echo "  help             - Show this help"

.PHONY: all test test-parallel clean benchmark benchmark-parallel install uninstall help
