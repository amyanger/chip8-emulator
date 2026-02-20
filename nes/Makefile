CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 $(shell sdl2-config --cflags) -I../6502/src
LDFLAGS = $(shell sdl2-config --libs)

CPU_SRC = ../6502/src/cpu6502.c ../6502/src/opcodes.c
NES_SRC = src/main.c src/nes.c src/ppu.c src/cartridge.c src/platform_nes.c

nes: $(NES_SRC) $(CPU_SRC)
	$(CC) $(CFLAGS) -o $@ $(NES_SRC) $(CPU_SRC) $(LDFLAGS)

clean:
	rm -f nes

.PHONY: clean
