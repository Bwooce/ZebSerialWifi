# Host unit tests for the pure parsers in main/. No ESP-IDF needed.
# Apple clang 21 with the MacOSX27 SDK fails to link these tests (libSystem.tbd), so prefer Homebrew LLVM when present.
ifeq ($(origin CC),default)
LLVM_CLANG := $(wildcard /opt/homebrew/opt/llvm/bin/clang)
ifneq ($(LLVM_CLANG),)
CC = $(LLVM_CLANG)
endif
endif
CFLAGS = -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -I../main
SRCS = test_main.c ../main/zpl_status.c ../main/urlcodec.c ../main/netcfg.c

all: build/test_main
	./build/test_main

build/test_main: $(SRCS) ../main/zpl_status.h ../main/urlcodec.h ../main/netcfg.h
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(SRCS)

clean:
	rm -rf build

.PHONY: all clean
