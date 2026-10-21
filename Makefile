# make        release build (-O2) of ./bookkeeping
# make debug  build/debug/bookkeeping with -g and the address/undefined sanitizers
# make test   unit tests and tests/e2e.sh, everything built with -fsanitize=address,undefined -g
# make clean  removes everything that was built

CC ?= cc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L
SANITIZE = -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer

SRC = $(wildcard src/*.c)
LIB_SRC = $(filter-out src/main.c,$(SRC))
HEADERS = $(wildcard include/*.h)
TESTS = $(patsubst tests/%.c,build/test/%,$(wildcard tests/test_*.c))

# leaks are errors, and any undefined behaviour stops the program with a report
export ASAN_OPTIONS = detect_leaks=1:abort_on_error=1
export UBSAN_OPTIONS = print_stacktrace=1:halt_on_error=1

.PHONY: all debug test clean

all: bookkeeping

bookkeeping: $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) -O2 $(SRC) -o $@

debug: build/debug/bookkeeping

build/debug/bookkeeping: $(SRC) $(HEADERS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -O0 $(SANITIZE) $(SRC) -o $@

build/test/bookkeeping: $(SRC) $(HEADERS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -O1 $(SANITIZE) $(SRC) -o $@

build/test/test_%: tests/test_%.c tests/test_util.h $(LIB_SRC) $(HEADERS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -O1 $(SANITIZE) $< $(LIB_SRC) -o $@

test: build/test/bookkeeping $(TESTS)
	@for t in $(TESTS); do echo "== $$t"; ./$$t || exit 1; done
	sh tests/e2e.sh build/test/bookkeeping

clean:
	rm -rf bookkeeping build
