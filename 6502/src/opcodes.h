#ifndef OPCODES_H
#define OPCODES_H

#include "cpu6502.h"

typedef void (*opcode_fn)(cpu6502_t *cpu);

/* 256-entry dispatch table indexed by opcode byte */
extern const opcode_fn opcode_table[256];

/* Base cycle count for each opcode */
extern const uint8_t opcode_cycles[256];

/* Mnemonic names for debug/disassembly */
extern const char *opcode_names[256];

#endif
