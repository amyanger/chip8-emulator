#ifndef CHIP8_H
#define CHIP8_H

#include <stdint.h>
#include <stdbool.h>

#define CHIP8_MEMORY_SIZE 4096
#define CHIP8_DISPLAY_WIDTH 64
#define CHIP8_DISPLAY_HEIGHT 32
#define CHIP8_REGISTER_COUNT 16
#define CHIP8_STACK_SIZE 16
#define CHIP8_KEYPAD_SIZE 16
#define CHIP8_PROGRAM_START 0x200

typedef struct {
    uint8_t memory[CHIP8_MEMORY_SIZE];
    uint8_t V[CHIP8_REGISTER_COUNT];       // general purpose registers V0-VF
    uint16_t I;                             // index register
    uint16_t pc;                            // program counter
    uint8_t display[CHIP8_DISPLAY_WIDTH * CHIP8_DISPLAY_HEIGHT];
    uint16_t stack[CHIP8_STACK_SIZE];
    uint8_t sp;                             // stack pointer
    uint8_t keypad[CHIP8_KEYPAD_SIZE];
    uint8_t delay_timer;
    uint8_t sound_timer;
    bool draw_flag;
} chip8_t;

void chip8_init(chip8_t *chip);
bool chip8_load_rom(chip8_t *chip, const char *path);
void chip8_cycle(chip8_t *chip);
void chip8_tick_timers(chip8_t *chip);

#endif
