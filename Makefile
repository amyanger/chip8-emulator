CC = gcc
CFLAGS = -Wall -Wextra -std=c99 $(shell sdl2-config --cflags)
LDFLAGS = $(shell sdl2-config --libs)
SRC = src/main.c src/chip8.c src/platform.c
TARGET = chip8

$(TARGET): $(SRC) src/chip8.h src/platform.h
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: clean
