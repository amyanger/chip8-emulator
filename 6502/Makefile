CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2

SRC = src/main.c src/cpu6502.c src/opcodes.c src/bus.c
TEST_SRC = test/test_main.c test/test_opcodes.c src/cpu6502.c src/opcodes.c src/bus.c

cpu6502: $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

test_cpu6502: $(TEST_SRC)
	$(CC) $(CFLAGS) -o $@ $^

test: test_cpu6502
	./test_cpu6502

clean:
	rm -f cpu6502 test_cpu6502

.PHONY: test clean
