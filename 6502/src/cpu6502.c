#include "cpu6502.h"
#include "opcodes.h"

void cpu6502_init(cpu6502_t *cpu, bus_read_fn read, bus_write_fn write, void *ctx)
{
    /* Zero all fields first */
    cpu->a = 0;
    cpu->x = 0;
    cpu->y = 0;
    cpu->sp = 0;
    cpu->pc = 0;
    cpu->cycles = 0;
    cpu->halted = false;
    cpu->page_crossed = false;

    /* Bit 5 (unused) is always set; I flag set at init */
    cpu->status = CPU_FLAG_U | CPU_FLAG_I;

    /* Install bus function pointers */
    cpu->read = read;
    cpu->write = write;
    cpu->bus_ctx = ctx;
}

void cpu6502_reset(cpu6502_t *cpu)
{
    /* Read reset vector from $FFFC/$FFFD (little-endian) */
    uint16_t lo = cpu_read(cpu, 0xFFFC);
    uint16_t hi = cpu_read(cpu, 0xFFFD);
    cpu->pc = (hi << 8) | lo;

    /* SP decremented by 3 during reset sequence (ends at $FD) */
    cpu->sp = 0xFD;

    /* Set interrupt disable, ensure bit 5 always set */
    cpu->status |= CPU_FLAG_I;
    cpu->status |= CPU_FLAG_U;

    cpu->halted = false;

    /* Reset takes 7 cycles */
    cpu->cycles += 7;
}

void cpu6502_step(cpu6502_t *cpu)
{
    if (cpu->halted) return;

    cpu->page_crossed = false;

    uint8_t opcode = cpu_read(cpu, cpu->pc++);

    /* Add base cycle count for this opcode */
    cpu->cycles += opcode_cycles[opcode];

    /* Dispatch to the opcode handler */
    opcode_table[opcode](cpu);
}

void cpu6502_irq(cpu6502_t *cpu)
{
    /* IRQ is masked when the I flag is set */
    if (cpu_get_flag(cpu, CPU_FLAG_I)) return;

    /* Push PC (high byte first, then low byte) */
    cpu_push16(cpu, cpu->pc);

    /* Push status with B=0 and U=1 */
    uint8_t flags = cpu->status;
    flags &= ~CPU_FLAG_B;  /* B clear for hardware interrupt */
    flags |= CPU_FLAG_U;   /* Bit 5 always set */
    cpu_push(cpu, flags);

    /* Set interrupt disable to prevent re-entry */
    cpu_set_flag(cpu, CPU_FLAG_I, true);

    /* Read IRQ vector from $FFFE/$FFFF */
    uint16_t lo = cpu_read(cpu, 0xFFFE);
    uint16_t hi = cpu_read(cpu, 0xFFFF);
    cpu->pc = (hi << 8) | lo;

    cpu->cycles += 7;
}

void cpu6502_nmi(cpu6502_t *cpu)
{
    /* NMI is NOT maskable -- always fires regardless of I flag */

    /* Push PC (high byte first, then low byte) */
    cpu_push16(cpu, cpu->pc);

    /* Push status with B=0 and U=1 */
    uint8_t flags = cpu->status;
    flags &= ~CPU_FLAG_B;  /* B clear for hardware interrupt */
    flags |= CPU_FLAG_U;   /* Bit 5 always set */
    cpu_push(cpu, flags);

    /* Set interrupt disable */
    cpu_set_flag(cpu, CPU_FLAG_I, true);

    /* Read NMI vector from $FFFA/$FFFB */
    uint16_t lo = cpu_read(cpu, 0xFFFA);
    uint16_t hi = cpu_read(cpu, 0xFFFB);
    cpu->pc = (hi << 8) | lo;

    cpu->cycles += 7;
}
