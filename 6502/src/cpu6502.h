#ifndef CPU6502_H
#define CPU6502_H

#include <stdint.h>
#include <stdbool.h>
#include "bus.h"

/* Status flag bit positions (match 6502 hardware layout) */
#define CPU_FLAG_C 0x01  /* bit 0: carry */
#define CPU_FLAG_Z 0x02  /* bit 1: zero */
#define CPU_FLAG_I 0x04  /* bit 2: interrupt disable */
#define CPU_FLAG_D 0x08  /* bit 3: decimal mode */
#define CPU_FLAG_B 0x10  /* bit 4: break (only on stack, not in register) */
#define CPU_FLAG_U 0x20  /* bit 5: unused (always 1) */
#define CPU_FLAG_V 0x40  /* bit 6: overflow */
#define CPU_FLAG_N 0x80  /* bit 7: negative */

typedef struct {
    /* Registers */
    uint8_t a;       /* accumulator */
    uint8_t x;       /* X index */
    uint8_t y;       /* Y index */
    uint8_t sp;      /* stack pointer (offset into page $01) */
    uint16_t pc;     /* program counter */
    uint8_t status;  /* processor status (NV-BDIZC) */

    /* Timing */
    uint64_t cycles;

    /* State */
    bool halted;
    bool page_crossed;  /* set by addressing helpers, consumed by handlers */

    /* Bus access */
    bus_read_fn read;
    bus_write_fn write;
    void *bus_ctx;
} cpu6502_t;

/* Core API */
void cpu6502_init(cpu6502_t *cpu, bus_read_fn read, bus_write_fn write, void *ctx);
void cpu6502_reset(cpu6502_t *cpu);
void cpu6502_step(cpu6502_t *cpu);
void cpu6502_irq(cpu6502_t *cpu);
void cpu6502_nmi(cpu6502_t *cpu);

/* Flag helpers (used by opcodes.c) */
static inline void cpu_set_flag(cpu6502_t *cpu, uint8_t flag, bool val) {
    if (val) cpu->status |= flag;
    else cpu->status &= ~flag;
}

static inline bool cpu_get_flag(cpu6502_t *cpu, uint8_t flag) {
    return (cpu->status & flag) != 0;
}

static inline void cpu_set_nz(cpu6502_t *cpu, uint8_t val) {
    cpu_set_flag(cpu, CPU_FLAG_N, val & 0x80);
    cpu_set_flag(cpu, CPU_FLAG_Z, val == 0);
}

/* Stack helpers */
static inline void cpu_push(cpu6502_t *cpu, uint8_t val) {
    cpu->write(cpu->bus_ctx, 0x0100 + cpu->sp, val);
    cpu->sp--;
}

static inline uint8_t cpu_pull(cpu6502_t *cpu) {
    cpu->sp++;
    return cpu->read(cpu->bus_ctx, 0x0100 + cpu->sp);
}

static inline void cpu_push16(cpu6502_t *cpu, uint16_t val) {
    cpu_push(cpu, (val >> 8) & 0xFF);
    cpu_push(cpu, val & 0xFF);
}

static inline uint16_t cpu_pull16(cpu6502_t *cpu) {
    uint16_t lo = cpu_pull(cpu);
    uint16_t hi = cpu_pull(cpu);
    return (hi << 8) | lo;
}

/* Bus read helper */
static inline uint8_t cpu_read(cpu6502_t *cpu, uint16_t addr) {
    return cpu->read(cpu->bus_ctx, addr);
}

static inline void cpu_write(cpu6502_t *cpu, uint16_t addr, uint8_t val) {
    cpu->write(cpu->bus_ctx, addr, val);
}

#endif
