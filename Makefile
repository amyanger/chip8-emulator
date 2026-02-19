CC = gcc
CFLAGS = -Wall -Wextra -std=c99
SRC = src/main.c src/chip8.c
TARGET = chip8

$(TARGET): $(SRC) src/chip8.h
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: clean
