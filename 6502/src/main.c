#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "bus.h"
#include "cpu6502.h"
#include "opcodes.h"

#define CYCLE_LIMIT 100000000ULL  /* 100 million cycles */

static void print_usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [-v] <binary> [base_addr] [start_addr]\n"
        "  binary      Path to 6502 binary file\n"
        "  base_addr   Load address in hex (default: 0000)\n"
        "  start_addr  Override PC in hex (default: read reset vector)\n"
        "  -v          Verbose per-instruction trace\n",
        prog);
}

static void print_status_flags(uint8_t status)
{
    printf("%c%c-%c%c%c%c%c",
        (status & CPU_FLAG_N) ? 'N' : 'n',
        (status & CPU_FLAG_V) ? 'V' : 'v',
        (status & CPU_FLAG_B) ? 'B' : 'b',
        (status & CPU_FLAG_D) ? 'D' : 'd',
        (status & CPU_FLAG_I) ? 'I' : 'i',
        (status & CPU_FLAG_Z) ? 'Z' : 'z',
        (status & CPU_FLAG_C) ? 'C' : 'c');
}

static void print_registers(cpu6502_t *cpu)
{
    printf("A:%02X X:%02X Y:%02X SP:%02X PC:%04X P:%02X [",
        cpu->a, cpu->x, cpu->y, cpu->sp, cpu->pc, cpu->status);
    print_status_flags(cpu->status);
    printf("] CYC:%llu\n", (unsigned long long)cpu->cycles);
}

/* Print a trace line in nestest.log-compatible format:
 * PC  OPCODE OPERANDS  MNEMONIC  A:XX X:XX Y:XX P:XX SP:XX CYC:NNNNN */
static void trace_instruction(cpu6502_t *cpu)
{
    uint8_t opcode = cpu_read(cpu, cpu->pc);
    const char *name = opcode_names[opcode] ? opcode_names[opcode] : "???";

    printf("%04X  %02X", cpu->pc, opcode);

    /* Peek at next bytes for display (do not advance PC) */
    uint8_t b1 = cpu_read(cpu, cpu->pc + 1);
    uint8_t b2 = cpu_read(cpu, cpu->pc + 2);

    /* Determine instruction length from addressing mode heuristics.
     * This is approximate -- accurate enough for trace output.
     * Most common sizes based on opcode patterns. */
    int len = 1;

    /* Use the cycle count as a rough proxy for instruction length:
     * - 2-cycle instructions are mostly implied/accumulator (1 byte)
     * - Instructions with immediate/zeropage operands are 2 bytes
     * - Absolute/indirect operands are 3 bytes
     * A more accurate approach would use an addressing mode table,
     * but this works for trace output. */
    uint8_t cyc = opcode_cycles[opcode];
    (void)cyc;

    /* Determine length by opcode group patterns.
     * Low nibble patterns that indicate instruction length: */
    uint8_t lo = opcode & 0x1F;
    if (opcode == 0x00 || opcode == 0x20 || opcode == 0x4C ||
        opcode == 0x6C || opcode == 0x40 || opcode == 0x60) {
        /* BRK=1(+padding), JSR=3, JMP abs=3, JMP ind=3, RTI=1, RTS=1 */
        if (opcode == 0x20 || opcode == 0x4C || opcode == 0x6C)
            len = 3;
        else
            len = 1;
    } else if ((lo & 0x0F) == 0x09 || (lo & 0x0F) == 0x0B) {
        /* Immediate modes for many instructions */
        len = 2;
    } else {
        /* General heuristic based on addressing mode grouping */
        uint8_t aaa = (opcode >> 5) & 0x07;
        uint8_t bbb = (opcode >> 2) & 0x07;
        uint8_t cc = opcode & 0x03;
        (void)aaa;

        if (cc == 0x01) {
            /* cc=01 group: ORA, AND, EOR, ADC, STA, LDA, CMP, SBC */
            switch (bbb) {
                case 0: len = 2; break;  /* (zp,X) */
                case 1: len = 2; break;  /* zp */
                case 2: len = 2; break;  /* #imm */
                case 3: len = 3; break;  /* abs */
                case 4: len = 2; break;  /* (zp),Y */
                case 5: len = 2; break;  /* zp,X */
                case 6: len = 3; break;  /* abs,Y */
                case 7: len = 3; break;  /* abs,X */
                default: len = 1; break;
            }
        } else if (cc == 0x02) {
            /* cc=10 group: ASL, ROL, LSR, ROR, STX, LDX, DEC, INC */
            switch (bbb) {
                case 0: len = 2; break;  /* #imm */
                case 1: len = 2; break;  /* zp */
                case 2: len = 1; break;  /* accumulator/implied */
                case 3: len = 3; break;  /* abs */
                case 5: len = 2; break;  /* zp,X (or zp,Y) */
                case 7: len = 3; break;  /* abs,X (or abs,Y) */
                default: len = 1; break;
            }
        } else if (cc == 0x00) {
            /* cc=00 group: BIT, JMP, STY, LDY, CPY, CPX + branches */
            switch (bbb) {
                case 0: len = 2; break;  /* #imm */
                case 1: len = 2; break;  /* zp */
                case 2: len = 1; break;  /* implied */
                case 3: len = 3; break;  /* abs */
                case 4: len = 2; break;  /* relative (branches) */
                case 5: len = 2; break;  /* zp,X */
                case 7: len = 3; break;  /* abs,X */
                default: len = 1; break;
            }
        } else {
            /* cc=11: illegal opcodes, default to 1 */
            len = 1;
        }
    }

    /* Print operand bytes */
    if (len == 1)
        printf("        ");
    else if (len == 2)
        printf(" %02X     ", b1);
    else
        printf(" %02X %02X  ", b1, b2);

    /* Mnemonic */
    printf("%s", name);

    /* Pad to register column */
    printf("%*s", 28 - (int)strlen(name), "");

    /* Register state BEFORE execution */
    printf("A:%02X X:%02X Y:%02X P:%02X SP:%02X CYC:%llu\n",
        cpu->a, cpu->x, cpu->y, cpu->status, cpu->sp,
        (unsigned long long)cpu->cycles);
}

int main(int argc, char *argv[])
{
    bool verbose = false;
    int arg_start = 1;

    /* Parse -v flag */
    if (argc > 1 && strcmp(argv[1], "-v") == 0) {
        verbose = true;
        arg_start = 2;
    }

    int remaining = argc - arg_start;
    if (remaining < 1 || remaining > 3) {
        print_usage(argv[0]);
        exit(1);
    }

    const char *binary_path = argv[arg_start];
    uint16_t base_addr = 0x0000;
    bool override_pc = false;
    uint16_t start_addr = 0;

    if (remaining >= 2) {
        unsigned long val = strtoul(argv[arg_start + 1], NULL, 16);
        if (val > 0xFFFF) {
            fprintf(stderr, "Error: base_addr %lX exceeds 16-bit range\n", val);
            exit(1);
        }
        base_addr = (uint16_t)val;
    }

    if (remaining >= 3) {
        unsigned long val = strtoul(argv[arg_start + 2], NULL, 16);
        if (val > 0xFFFF) {
            fprintf(stderr, "Error: start_addr %lX exceeds 16-bit range\n", val);
            exit(1);
        }
        start_addr = (uint16_t)val;
        override_pc = true;
    }

    /* Initialize bus and load binary */
    bus_flat_t bus;
    bus_flat_init(&bus);

    if (!bus_flat_load(&bus, binary_path, base_addr)) {
        exit(1);
    }

    /* Initialize and reset CPU */
    cpu6502_t cpu;
    cpu6502_init(&cpu, bus_flat_read, bus_flat_write, &bus);
    cpu6502_reset(&cpu);

    /* Override PC if start address was provided */
    if (override_pc) {
        cpu.pc = start_addr;
    }

    printf("Loaded '%s' at $%04X, PC=$%04X\n", binary_path, base_addr, cpu.pc);

    /* Main execution loop */
    uint16_t prev_pc;
    while (!cpu.halted && cpu.cycles < CYCLE_LIMIT) {
        prev_pc = cpu.pc;

        if (verbose) {
            trace_instruction(&cpu);
        }

        cpu6502_step(&cpu);

        /* Detect trap: PC pointing to itself (e.g., JMP *) */
        if (cpu.pc == prev_pc) {
            printf("Trap detected at $%04X\n", cpu.pc);
            break;
        }
    }

    if (cpu.cycles >= CYCLE_LIMIT) {
        printf("Cycle limit reached (%llu cycles)\n",
            (unsigned long long)CYCLE_LIMIT);
    }

    if (cpu.halted) {
        printf("CPU halted\n");
    }

    /* Final register dump */
    printf("\nFinal state:\n");
    print_registers(&cpu);

    return 0;
}
