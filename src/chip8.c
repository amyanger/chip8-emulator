#include "chip8.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

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
    srand(time(NULL));
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

    size_t bytes_read = fread(&chip->memory[CHIP8_PROGRAM_START], 1, size, f);
    fclose(f);

    if ((long)bytes_read != size) {
        fprintf(stderr, "ROM read incomplete: got %zu of %ld bytes\n", bytes_read, size);
        return false;
    }

    printf("Loaded ROM: %s (%ld bytes)\n", path, size);
    return true;
}

void chip8_cycle(chip8_t *chip) {
    /* Bounds check: PC must have room for a 2-byte opcode */
    if (chip->pc > CHIP8_MEMORY_SIZE - 2) {
        fprintf(stderr, "PC out of bounds: 0x%04X\n", chip->pc);
        return;
    }

    /* Fetch: read two bytes from memory at PC (big-endian) */
    uint16_t opcode = (chip->memory[chip->pc] << 8) | chip->memory[chip->pc + 1];

    /* Common bit extractions used by many instructions */
    uint8_t x   = (opcode >> 8) & 0x0F;   /* second nibble  */
    uint8_t y   = (opcode >> 4) & 0x0F;   /* third nibble   */
    uint8_t n   =  opcode       & 0x0F;   /* fourth nibble  */
    uint8_t nn  =  opcode       & 0xFF;   /* lower byte     */
    uint16_t nnn =  opcode       & 0x0FFF; /* lower 12 bits  */

    /* Advance PC past this instruction before executing.
     * Instructions that modify PC (jumps, skips, calls, FX0A) will
     * override this as needed. */
    chip->pc += 2;

    /* Decode and execute */
    switch (opcode & 0xF000) {

    case 0x0000:
        switch (opcode) {
        case 0x00E0: /* 00E0 - Clear display */
            memset(chip->display, 0, sizeof(chip->display));
            chip->draw_flag = true;
            break;

        case 0x00EE: /* 00EE - Return from subroutine */
            if (chip->sp == 0) {
                fprintf(stderr, "Stack underflow!\n");
                break;
            }
            chip->sp--;
            chip->pc = chip->stack[chip->sp];
            break;

        default:
            fprintf(stderr, "Unknown opcode: 0x%04X\n", opcode);
            break;
        }
        break;

    case 0x1000: /* 1NNN - Jump to NNN */
        chip->pc = nnn;
        break;

    case 0x2000: /* 2NNN - Call subroutine at NNN */
        if (chip->sp >= CHIP8_STACK_SIZE) {
            fprintf(stderr, "Stack overflow!\n");
            break;
        }
        chip->stack[chip->sp] = chip->pc;
        chip->sp++;
        chip->pc = nnn;
        break;

    case 0x3000: /* 3XNN - Skip next if VX == NN */
        if (chip->V[x] == nn)
            chip->pc += 2;
        break;

    case 0x4000: /* 4XNN - Skip next if VX != NN */
        if (chip->V[x] != nn)
            chip->pc += 2;
        break;

    case 0x5000: /* 5XY0 - Skip next if VX == VY */
        if (chip->V[x] == chip->V[y])
            chip->pc += 2;
        break;

    case 0x6000: /* 6XNN - Set VX = NN */
        chip->V[x] = nn;
        break;

    case 0x7000: /* 7XNN - Add NN to VX (no carry flag) */
        chip->V[x] += nn;
        break;

    case 0x8000:
        switch (n) {
        case 0x0: /* 8XY0 - Set VX = VY */
            chip->V[x] = chip->V[y];
            break;

        case 0x1: /* 8XY1 - VX = VX | VY */
            chip->V[x] |= chip->V[y];
            break;

        case 0x2: /* 8XY2 - VX = VX & VY */
            chip->V[x] &= chip->V[y];
            break;

        case 0x3: /* 8XY3 - VX = VX ^ VY */
            chip->V[x] ^= chip->V[y];
            break;

        case 0x4: { /* 8XY4 - VX += VY, VF = carry */
            uint16_t sum = chip->V[x] + chip->V[y];
            chip->V[x] = sum & 0xFF;
            chip->V[0xF] = (sum > 0xFF) ? 1 : 0;
            break;
        }

        case 0x5: { /* 8XY5 - VX -= VY, VF = NOT borrow */
            uint8_t flag = (chip->V[x] >= chip->V[y]) ? 1 : 0;
            chip->V[x] -= chip->V[y];
            chip->V[0xF] = flag;
            break;
        }

        case 0x6: { /* 8XY6 - VX >>= 1, VF = LSB before shift */
            uint8_t lsb = chip->V[x] & 0x01;
            chip->V[x] >>= 1;
            chip->V[0xF] = lsb;
            break;
        }

        case 0x7: { /* 8XY7 - VX = VY - VX, VF = NOT borrow */
            uint8_t flag = (chip->V[y] >= chip->V[x]) ? 1 : 0;
            chip->V[x] = chip->V[y] - chip->V[x];
            chip->V[0xF] = flag;
            break;
        }

        case 0xE: { /* 8XYE - VX <<= 1, VF = MSB before shift */
            uint8_t msb = (chip->V[x] >> 7) & 0x01;
            chip->V[x] <<= 1;
            chip->V[0xF] = msb;
            break;
        }

        default:
            fprintf(stderr, "Unknown opcode: 0x%04X\n", opcode);
            break;
        }
        break;

    case 0x9000: /* 9XY0 - Skip next if VX != VY */
        if (chip->V[x] != chip->V[y])
            chip->pc += 2;
        break;

    case 0xA000: /* ANNN - Set I = NNN */
        chip->I = nnn;
        break;

    case 0xB000: /* BNNN - Jump to NNN + V0 */
        chip->pc = nnn + chip->V[0];
        break;

    case 0xC000: /* CXNN - VX = random byte & NN */
        chip->V[x] = (rand() & 0xFF) & nn;
        break;

    case 0xD000: { /* DXYN - Draw sprite at (VX, VY), N bytes tall */
        uint8_t xpos = chip->V[x] % CHIP8_DISPLAY_WIDTH;
        uint8_t ypos = chip->V[y] % CHIP8_DISPLAY_HEIGHT;
        chip->V[0xF] = 0;

        for (uint8_t row = 0; row < n; row++) {
            uint8_t py = ypos + row;
            if (py >= CHIP8_DISPLAY_HEIGHT) break;

            uint8_t sprite_byte = chip->memory[(chip->I + row) & 0xFFF];

            for (uint8_t col = 0; col < 8; col++) {
                uint8_t px = xpos + col;
                if (px >= CHIP8_DISPLAY_WIDTH) break;

                /* Check if this bit in the sprite row is set */
                if ((sprite_byte & (0x80 >> col)) != 0) {
                    uint16_t idx = py * CHIP8_DISPLAY_WIDTH + px;

                    /* Collision: pixel was on, now will be turned off */
                    if (chip->display[idx] == 1)
                        chip->V[0xF] = 1;

                    chip->display[idx] ^= 1;
                }
            }
        }

        chip->draw_flag = true;
        break;
    }

    case 0xE000:
        switch (nn) {
        case 0x9E: /* EX9E - Skip next if key VX is pressed */
            if (chip->keypad[chip->V[x] & 0x0F])
                chip->pc += 2;
            break;

        case 0xA1: /* EXA1 - Skip next if key VX is NOT pressed */
            if (!chip->keypad[chip->V[x] & 0x0F])
                chip->pc += 2;
            break;

        default:
            fprintf(stderr, "Unknown opcode: 0x%04X\n", opcode);
            break;
        }
        break;

    case 0xF000:
        switch (nn) {
        case 0x07: /* FX07 - VX = delay timer */
            chip->V[x] = chip->delay_timer;
            break;

        case 0x0A: { /* FX0A - Wait for key press, store in VX */
            bool key_pressed = false;
            for (uint8_t i = 0; i < CHIP8_KEYPAD_SIZE; i++) {
                if (chip->keypad[i]) {
                    chip->V[x] = i;
                    key_pressed = true;
                    break;
                }
            }
            /* If no key was pressed, rewind PC to re-execute this
             * instruction on the next cycle (blocking wait). */
            if (!key_pressed)
                chip->pc -= 2;
            break;
        }

        case 0x15: /* FX15 - Set delay timer = VX */
            chip->delay_timer = chip->V[x];
            break;

        case 0x18: /* FX18 - Set sound timer = VX */
            chip->sound_timer = chip->V[x];
            break;

        case 0x1E: /* FX1E - I += VX */
            chip->I += chip->V[x];
            chip->I &= 0xFFF;
            break;

        case 0x29: /* FX29 - I = address of font character VX */
            chip->I = (chip->V[x] & 0x0F) * 5;
            break;

        case 0x33: /* FX33 - Store BCD of VX at I, I+1, I+2 */
            chip->memory[chip->I & 0xFFF]       = chip->V[x] / 100;
            chip->memory[(chip->I + 1) & 0xFFF] = (chip->V[x] / 10) % 10;
            chip->memory[(chip->I + 2) & 0xFFF] = chip->V[x] % 10;
            break;

        case 0x55: /* FX55 - Store V0..VX in memory starting at I */
            for (uint8_t i = 0; i <= x; i++)
                chip->memory[(chip->I + i) & 0xFFF] = chip->V[i];
            break;

        case 0x65: /* FX65 - Load V0..VX from memory starting at I */
            for (uint8_t i = 0; i <= x; i++)
                chip->V[i] = chip->memory[(chip->I + i) & 0xFFF];
            break;

        default:
            fprintf(stderr, "Unknown opcode: 0x%04X\n", opcode);
            break;
        }
        break;

    default:
        fprintf(stderr, "Unknown opcode: 0x%04X\n", opcode);
        break;
    }
}

void chip8_tick_timers(chip8_t *chip) {
    if (chip->delay_timer > 0) chip->delay_timer--;
    if (chip->sound_timer > 0) chip->sound_timer--;
}
