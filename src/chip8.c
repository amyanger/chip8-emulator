#include "chip8.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const uint8_t fontset[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80, // F
};

void chip8_init(chip8_t *chip) {
    memset(chip, 0, sizeof(chip8_t));
    chip->pc = CHIP8_PROGRAM_START;
    memcpy(chip->memory, fontset, sizeof(fontset));
}

bool chip8_load_rom(chip8_t *chip, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open ROM: %s\n", path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    if (size > CHIP8_MEMORY_SIZE - CHIP8_PROGRAM_START) {
        fprintf(stderr, "ROM too large: %ld bytes\n", size);
        fclose(f);
        return false;
    }

    fread(&chip->memory[CHIP8_PROGRAM_START], 1, size, f);
    fclose(f);
    printf("Loaded ROM: %s (%ld bytes)\n", path, size);
    return true;
}

void chip8_cycle(chip8_t *chip) {
    // TODO: fetch, decode, execute
}

void chip8_tick_timers(chip8_t *chip) {
    if (chip->delay_timer > 0) chip->delay_timer--;
    if (chip->sound_timer > 0) chip->sound_timer--;
}
