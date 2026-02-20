/*
 * opcodes.c — 6502 opcode handlers and dispatch tables
 *
 * Contains:
 *   1. Addressing mode helpers (static)
 *   2. Shared instruction core functions (static)
 *   3. Individual opcode handler functions (static)
 *   4. Exported tables: opcode_table, opcode_cycles, opcode_names
 */

#include <stdio.h>
#include <stdint.h>
#include "cpu6502.h"
#include "opcodes.h"

/* ======================================================================
 * 1. Addressing mode helpers
 *
 * Each returns the effective address and advances PC as needed.
 * Page crossing detection is set on cpu->page_crossed for abx, aby, izy.
 * ====================================================================== */

/* Immediate: operand is the byte at PC */
static uint16_t addr_imm(cpu6502_t *cpu) {
    return cpu->pc++;
}

/* Zero page: operand address is a single byte (0x00xx) */
static uint16_t addr_zpg(cpu6502_t *cpu) {
    return cpu_read(cpu, cpu->pc++);
}

/* Zero page,X: (byte + X) wrapped to zero page */
static uint16_t addr_zpx(cpu6502_t *cpu) {
    return (cpu_read(cpu, cpu->pc++) + cpu->x) & 0xFF;
}

/* Zero page,Y: (byte + Y) wrapped to zero page */
static uint16_t addr_zpy(cpu6502_t *cpu) {
    return (cpu_read(cpu, cpu->pc++) + cpu->y) & 0xFF;
}

/* Absolute: two-byte little-endian address */
static uint16_t addr_abs(cpu6502_t *cpu) {
    uint16_t lo = cpu_read(cpu, cpu->pc++);
    uint16_t hi = cpu_read(cpu, cpu->pc++);
    return (hi << 8) | lo;
}

/* Absolute,X: absolute + X with page crossing detection */
static uint16_t addr_abx(cpu6502_t *cpu) {
    uint16_t base = addr_abs(cpu);
    uint16_t addr = base + cpu->x;
    cpu->page_crossed = ((base & 0xFF00) != (addr & 0xFF00));
    return addr;
}

/* Absolute,Y: absolute + Y with page crossing detection */
static uint16_t addr_aby(cpu6502_t *cpu) {
    uint16_t base = addr_abs(cpu);
    uint16_t addr = base + cpu->y;
    cpu->page_crossed = ((base & 0xFF00) != (addr & 0xFF00));
    return addr;
}

/* (Indirect,X): zero-page pointer with X offset */
static uint16_t addr_izx(cpu6502_t *cpu) {
    uint8_t ptr = (cpu_read(cpu, cpu->pc++) + cpu->x) & 0xFF;
    uint16_t lo = cpu_read(cpu, ptr);
    uint16_t hi = cpu_read(cpu, (ptr + 1) & 0xFF);
    return (hi << 8) | lo;
}

/* (Indirect),Y: zero-page pointer, then add Y with page crossing detection */
static uint16_t addr_izy(cpu6502_t *cpu) {
    uint8_t ptr = cpu_read(cpu, cpu->pc++);
    uint16_t lo = cpu_read(cpu, ptr);
    uint16_t hi = cpu_read(cpu, (ptr + 1) & 0xFF);
    uint16_t base = (hi << 8) | lo;
    uint16_t addr = base + cpu->y;
    cpu->page_crossed = ((base & 0xFF00) != (addr & 0xFF00));
    return addr;
}

/* ======================================================================
 * 2. Shared instruction core functions
 * ====================================================================== */

/* ADC: add with carry, handles both binary and decimal (BCD) modes */
static void do_adc(cpu6502_t *cpu, uint8_t val) {
    if (cpu_get_flag(cpu, CPU_FLAG_D)) {
        /* Decimal mode (NMOS 6502 behavior) */
        uint8_t a = cpu->a;
        uint8_t carry = cpu_get_flag(cpu, CPU_FLAG_C) ? 1 : 0;

        /* Z flag based on binary result (NMOS quirk) */
        uint16_t bin = a + val + carry;
        cpu_set_flag(cpu, CPU_FLAG_Z, (bin & 0xFF) == 0);

        /* Low nybble */
        int al = (a & 0x0F) + (val & 0x0F) + carry;
        if (al > 9) al += 6;

        /* High nybble */
        int ah = (a >> 4) + (val >> 4) + (al > 0x0F ? 1 : 0);

        /* N and V flags set BEFORE high nybble BCD fixup */
        uint8_t partial = (uint8_t)((ah << 4) | (al & 0x0F));
        cpu_set_flag(cpu, CPU_FLAG_N, partial & 0x80);
        cpu_set_flag(cpu, CPU_FLAG_V, (~(a ^ val) & (a ^ (ah << 4)) & 0x80));

        if (ah > 9) ah += 6;
        cpu_set_flag(cpu, CPU_FLAG_C, ah > 0x0F);
        cpu->a = (uint8_t)(((ah & 0x0F) << 4) | (al & 0x0F));
    } else {
        /* Binary mode */
        uint16_t sum = cpu->a + val + (cpu_get_flag(cpu, CPU_FLAG_C) ? 1 : 0);
        cpu_set_flag(cpu, CPU_FLAG_V, (~(cpu->a ^ val) & (cpu->a ^ sum) & 0x80));
        cpu_set_flag(cpu, CPU_FLAG_C, sum > 0xFF);
        cpu->a = (uint8_t)(sum & 0xFF);
        cpu_set_nz(cpu, cpu->a);
    }
}

/* SBC: subtract with borrow, handles both binary and decimal modes */
static void do_sbc(cpu6502_t *cpu, uint8_t val) {
    if (cpu_get_flag(cpu, CPU_FLAG_D)) {
        /* Decimal mode (NMOS 6502 behavior) */
        uint8_t a = cpu->a;
        uint8_t borrow = cpu_get_flag(cpu, CPU_FLAG_C) ? 0 : 1;

        /* ALL flags based on binary result for SBC */
        uint16_t bin = a - val - borrow;
        cpu_set_flag(cpu, CPU_FLAG_C, !(bin & 0x100));
        cpu_set_flag(cpu, CPU_FLAG_Z, (bin & 0xFF) == 0);
        cpu_set_flag(cpu, CPU_FLAG_N, bin & 0x80);
        cpu_set_flag(cpu, CPU_FLAG_V, ((a ^ val) & (a ^ bin) & 0x80));

        int al = (a & 0x0F) - (val & 0x0F) - borrow;
        if (al < 0) al = ((al - 6) & 0x0F) - 0x10;
        int ah = (a >> 4) - (val >> 4) + (al < 0 ? -1 : 0);
        if (ah < 0) ah -= 6;
        cpu->a = (uint8_t)(((ah & 0x0F) << 4) | (al & 0x0F));
    } else {
        /* Binary mode: SBC is ADC with complement */
        do_adc(cpu, ~val);
    }
}

/* CMP: compare register with value, set N/Z/C */
static void do_cmp(cpu6502_t *cpu, uint8_t reg, uint8_t val) {
    uint16_t result = reg - val;
    cpu_set_flag(cpu, CPU_FLAG_C, reg >= val);
    cpu_set_flag(cpu, CPU_FLAG_Z, reg == val);
    cpu_set_flag(cpu, CPU_FLAG_N, result & 0x80);
}

/* AND: A &= val, set N/Z */
static void do_and(cpu6502_t *cpu, uint8_t val) {
    cpu->a &= val;
    cpu_set_nz(cpu, cpu->a);
}

/* ORA: A |= val, set N/Z */
static void do_ora(cpu6502_t *cpu, uint8_t val) {
    cpu->a |= val;
    cpu_set_nz(cpu, cpu->a);
}

/* EOR: A ^= val, set N/Z */
static void do_eor(cpu6502_t *cpu, uint8_t val) {
    cpu->a ^= val;
    cpu_set_nz(cpu, cpu->a);
}

/* ASL: arithmetic shift left, set N/Z/C, return result */
static uint8_t do_asl(cpu6502_t *cpu, uint8_t val) {
    cpu_set_flag(cpu, CPU_FLAG_C, val & 0x80);
    val <<= 1;
    cpu_set_nz(cpu, val);
    return val;
}

/* LSR: logical shift right, set N(=0)/Z/C, return result */
static uint8_t do_lsr(cpu6502_t *cpu, uint8_t val) {
    cpu_set_flag(cpu, CPU_FLAG_C, val & 0x01);
    val >>= 1;
    cpu_set_nz(cpu, val);
    return val;
}

/* ROL: rotate left through carry, return result */
static uint8_t do_rol(cpu6502_t *cpu, uint8_t val) {
    uint8_t old_carry = cpu_get_flag(cpu, CPU_FLAG_C) ? 1 : 0;
    cpu_set_flag(cpu, CPU_FLAG_C, val & 0x80);
    val = (uint8_t)((val << 1) | old_carry);
    cpu_set_nz(cpu, val);
    return val;
}

/* ROR: rotate right through carry, return result */
static uint8_t do_ror(cpu6502_t *cpu, uint8_t val) {
    uint8_t old_carry = cpu_get_flag(cpu, CPU_FLAG_C) ? 0x80 : 0;
    cpu_set_flag(cpu, CPU_FLAG_C, val & 0x01);
    val = (uint8_t)((val >> 1) | old_carry);
    cpu_set_nz(cpu, val);
    return val;
}

/* ======================================================================
 * 3. Individual opcode handlers
 *
 * Naming: op_<mnemonic>_<mode>
 *   modes: imm, zpg, zpx, zpy, abs, abx, aby, izx, izy, acc, ind
 * ====================================================================== */

/* --- Illegal opcode handler --- */

static void op_ill(cpu6502_t *cpu) {
    fprintf(stderr, "Illegal opcode at $%04X\n", cpu->pc - 1);
    cpu->halted = true;
}

/* ===== LDA ===== */

static void op_lda_imm(cpu6502_t *cpu) {
    cpu->a = cpu_read(cpu, addr_imm(cpu));
    cpu_set_nz(cpu, cpu->a);
}

static void op_lda_zpg(cpu6502_t *cpu) {
    cpu->a = cpu_read(cpu, addr_zpg(cpu));
    cpu_set_nz(cpu, cpu->a);
}

static void op_lda_zpx(cpu6502_t *cpu) {
    cpu->a = cpu_read(cpu, addr_zpx(cpu));
    cpu_set_nz(cpu, cpu->a);
}

static void op_lda_abs(cpu6502_t *cpu) {
    cpu->a = cpu_read(cpu, addr_abs(cpu));
    cpu_set_nz(cpu, cpu->a);
}

static void op_lda_abx(cpu6502_t *cpu) {
    cpu->a = cpu_read(cpu, addr_abx(cpu));
    cpu_set_nz(cpu, cpu->a);
    if (cpu->page_crossed) cpu->cycles++;
}

static void op_lda_aby(cpu6502_t *cpu) {
    cpu->a = cpu_read(cpu, addr_aby(cpu));
    cpu_set_nz(cpu, cpu->a);
    if (cpu->page_crossed) cpu->cycles++;
}

static void op_lda_izx(cpu6502_t *cpu) {
    cpu->a = cpu_read(cpu, addr_izx(cpu));
    cpu_set_nz(cpu, cpu->a);
}

static void op_lda_izy(cpu6502_t *cpu) {
    cpu->a = cpu_read(cpu, addr_izy(cpu));
    cpu_set_nz(cpu, cpu->a);
    if (cpu->page_crossed) cpu->cycles++;
}

/* ===== LDX ===== */

static void op_ldx_imm(cpu6502_t *cpu) {
    cpu->x = cpu_read(cpu, addr_imm(cpu));
    cpu_set_nz(cpu, cpu->x);
}

static void op_ldx_zpg(cpu6502_t *cpu) {
    cpu->x = cpu_read(cpu, addr_zpg(cpu));
    cpu_set_nz(cpu, cpu->x);
}

static void op_ldx_zpy(cpu6502_t *cpu) {
    cpu->x = cpu_read(cpu, addr_zpy(cpu));
    cpu_set_nz(cpu, cpu->x);
}

static void op_ldx_abs(cpu6502_t *cpu) {
    cpu->x = cpu_read(cpu, addr_abs(cpu));
    cpu_set_nz(cpu, cpu->x);
}

static void op_ldx_aby(cpu6502_t *cpu) {
    cpu->x = cpu_read(cpu, addr_aby(cpu));
    cpu_set_nz(cpu, cpu->x);
    if (cpu->page_crossed) cpu->cycles++;
}

/* ===== LDY ===== */

static void op_ldy_imm(cpu6502_t *cpu) {
    cpu->y = cpu_read(cpu, addr_imm(cpu));
    cpu_set_nz(cpu, cpu->y);
}

static void op_ldy_zpg(cpu6502_t *cpu) {
    cpu->y = cpu_read(cpu, addr_zpg(cpu));
    cpu_set_nz(cpu, cpu->y);
}

static void op_ldy_zpx(cpu6502_t *cpu) {
    cpu->y = cpu_read(cpu, addr_zpx(cpu));
    cpu_set_nz(cpu, cpu->y);
}

static void op_ldy_abs(cpu6502_t *cpu) {
    cpu->y = cpu_read(cpu, addr_abs(cpu));
    cpu_set_nz(cpu, cpu->y);
}

static void op_ldy_abx(cpu6502_t *cpu) {
    cpu->y = cpu_read(cpu, addr_abx(cpu));
    cpu_set_nz(cpu, cpu->y);
    if (cpu->page_crossed) cpu->cycles++;
}

/* ===== STA ===== */

static void op_sta_zpg(cpu6502_t *cpu) {
    cpu_write(cpu, addr_zpg(cpu), cpu->a);
}

static void op_sta_zpx(cpu6502_t *cpu) {
    cpu_write(cpu, addr_zpx(cpu), cpu->a);
}

static void op_sta_abs(cpu6502_t *cpu) {
    cpu_write(cpu, addr_abs(cpu), cpu->a);
}

static void op_sta_abx(cpu6502_t *cpu) {
    cpu_write(cpu, addr_abx(cpu), cpu->a);
}

static void op_sta_aby(cpu6502_t *cpu) {
    cpu_write(cpu, addr_aby(cpu), cpu->a);
}

static void op_sta_izx(cpu6502_t *cpu) {
    cpu_write(cpu, addr_izx(cpu), cpu->a);
}

static void op_sta_izy(cpu6502_t *cpu) {
    cpu_write(cpu, addr_izy(cpu), cpu->a);
}

/* ===== STX ===== */

static void op_stx_zpg(cpu6502_t *cpu) {
    cpu_write(cpu, addr_zpg(cpu), cpu->x);
}

static void op_stx_zpy(cpu6502_t *cpu) {
    cpu_write(cpu, addr_zpy(cpu), cpu->x);
}

static void op_stx_abs(cpu6502_t *cpu) {
    cpu_write(cpu, addr_abs(cpu), cpu->x);
}

/* ===== STY ===== */

static void op_sty_zpg(cpu6502_t *cpu) {
    cpu_write(cpu, addr_zpg(cpu), cpu->y);
}

static void op_sty_zpx(cpu6502_t *cpu) {
    cpu_write(cpu, addr_zpx(cpu), cpu->y);
}

static void op_sty_abs(cpu6502_t *cpu) {
    cpu_write(cpu, addr_abs(cpu), cpu->y);
}

/* ===== ADC ===== */

static void op_adc_imm(cpu6502_t *cpu) {
    do_adc(cpu, cpu_read(cpu, addr_imm(cpu)));
}

static void op_adc_zpg(cpu6502_t *cpu) {
    do_adc(cpu, cpu_read(cpu, addr_zpg(cpu)));
}

static void op_adc_zpx(cpu6502_t *cpu) {
    do_adc(cpu, cpu_read(cpu, addr_zpx(cpu)));
}

static void op_adc_abs(cpu6502_t *cpu) {
    do_adc(cpu, cpu_read(cpu, addr_abs(cpu)));
}

static void op_adc_abx(cpu6502_t *cpu) {
    do_adc(cpu, cpu_read(cpu, addr_abx(cpu)));
    if (cpu->page_crossed) cpu->cycles++;
}

static void op_adc_aby(cpu6502_t *cpu) {
    do_adc(cpu, cpu_read(cpu, addr_aby(cpu)));
    if (cpu->page_crossed) cpu->cycles++;
}

static void op_adc_izx(cpu6502_t *cpu) {
    do_adc(cpu, cpu_read(cpu, addr_izx(cpu)));
}

static void op_adc_izy(cpu6502_t *cpu) {
    do_adc(cpu, cpu_read(cpu, addr_izy(cpu)));
    if (cpu->page_crossed) cpu->cycles++;
}

/* ===== SBC ===== */

static void op_sbc_imm(cpu6502_t *cpu) {
    do_sbc(cpu, cpu_read(cpu, addr_imm(cpu)));
}

static void op_sbc_zpg(cpu6502_t *cpu) {
    do_sbc(cpu, cpu_read(cpu, addr_zpg(cpu)));
}

static void op_sbc_zpx(cpu6502_t *cpu) {
    do_sbc(cpu, cpu_read(cpu, addr_zpx(cpu)));
}

static void op_sbc_abs(cpu6502_t *cpu) {
    do_sbc(cpu, cpu_read(cpu, addr_abs(cpu)));
}

static void op_sbc_abx(cpu6502_t *cpu) {
    do_sbc(cpu, cpu_read(cpu, addr_abx(cpu)));
    if (cpu->page_crossed) cpu->cycles++;
}

static void op_sbc_aby(cpu6502_t *cpu) {
    do_sbc(cpu, cpu_read(cpu, addr_aby(cpu)));
    if (cpu->page_crossed) cpu->cycles++;
}

static void op_sbc_izx(cpu6502_t *cpu) {
    do_sbc(cpu, cpu_read(cpu, addr_izx(cpu)));
}

static void op_sbc_izy(cpu6502_t *cpu) {
    do_sbc(cpu, cpu_read(cpu, addr_izy(cpu)));
    if (cpu->page_crossed) cpu->cycles++;
}

/* ===== CMP ===== */

static void op_cmp_imm(cpu6502_t *cpu) {
    do_cmp(cpu, cpu->a, cpu_read(cpu, addr_imm(cpu)));
}

static void op_cmp_zpg(cpu6502_t *cpu) {
    do_cmp(cpu, cpu->a, cpu_read(cpu, addr_zpg(cpu)));
}

static void op_cmp_zpx(cpu6502_t *cpu) {
    do_cmp(cpu, cpu->a, cpu_read(cpu, addr_zpx(cpu)));
}

static void op_cmp_abs(cpu6502_t *cpu) {
    do_cmp(cpu, cpu->a, cpu_read(cpu, addr_abs(cpu)));
}

static void op_cmp_abx(cpu6502_t *cpu) {
    do_cmp(cpu, cpu->a, cpu_read(cpu, addr_abx(cpu)));
    if (cpu->page_crossed) cpu->cycles++;
}

static void op_cmp_aby(cpu6502_t *cpu) {
    do_cmp(cpu, cpu->a, cpu_read(cpu, addr_aby(cpu)));
    if (cpu->page_crossed) cpu->cycles++;
}

static void op_cmp_izx(cpu6502_t *cpu) {
    do_cmp(cpu, cpu->a, cpu_read(cpu, addr_izx(cpu)));
}

static void op_cmp_izy(cpu6502_t *cpu) {
    do_cmp(cpu, cpu->a, cpu_read(cpu, addr_izy(cpu)));
    if (cpu->page_crossed) cpu->cycles++;
}

/* ===== CPX ===== */

static void op_cpx_imm(cpu6502_t *cpu) {
    do_cmp(cpu, cpu->x, cpu_read(cpu, addr_imm(cpu)));
}

static void op_cpx_zpg(cpu6502_t *cpu) {
    do_cmp(cpu, cpu->x, cpu_read(cpu, addr_zpg(cpu)));
}

static void op_cpx_abs(cpu6502_t *cpu) {
    do_cmp(cpu, cpu->x, cpu_read(cpu, addr_abs(cpu)));
}

/* ===== CPY ===== */

static void op_cpy_imm(cpu6502_t *cpu) {
    do_cmp(cpu, cpu->y, cpu_read(cpu, addr_imm(cpu)));
}

static void op_cpy_zpg(cpu6502_t *cpu) {
    do_cmp(cpu, cpu->y, cpu_read(cpu, addr_zpg(cpu)));
}

static void op_cpy_abs(cpu6502_t *cpu) {
    do_cmp(cpu, cpu->y, cpu_read(cpu, addr_abs(cpu)));
}

/* ===== AND ===== */

static void op_and_imm(cpu6502_t *cpu) {
    do_and(cpu, cpu_read(cpu, addr_imm(cpu)));
}

static void op_and_zpg(cpu6502_t *cpu) {
    do_and(cpu, cpu_read(cpu, addr_zpg(cpu)));
}

static void op_and_zpx(cpu6502_t *cpu) {
    do_and(cpu, cpu_read(cpu, addr_zpx(cpu)));
}

static void op_and_abs(cpu6502_t *cpu) {
    do_and(cpu, cpu_read(cpu, addr_abs(cpu)));
}

static void op_and_abx(cpu6502_t *cpu) {
    do_and(cpu, cpu_read(cpu, addr_abx(cpu)));
    if (cpu->page_crossed) cpu->cycles++;
}

static void op_and_aby(cpu6502_t *cpu) {
    do_and(cpu, cpu_read(cpu, addr_aby(cpu)));
    if (cpu->page_crossed) cpu->cycles++;
}

static void op_and_izx(cpu6502_t *cpu) {
    do_and(cpu, cpu_read(cpu, addr_izx(cpu)));
}

static void op_and_izy(cpu6502_t *cpu) {
    do_and(cpu, cpu_read(cpu, addr_izy(cpu)));
    if (cpu->page_crossed) cpu->cycles++;
}

/* ===== EOR ===== */

static void op_eor_imm(cpu6502_t *cpu) {
    do_eor(cpu, cpu_read(cpu, addr_imm(cpu)));
}

static void op_eor_zpg(cpu6502_t *cpu) {
    do_eor(cpu, cpu_read(cpu, addr_zpg(cpu)));
}

static void op_eor_zpx(cpu6502_t *cpu) {
    do_eor(cpu, cpu_read(cpu, addr_zpx(cpu)));
}

static void op_eor_abs(cpu6502_t *cpu) {
    do_eor(cpu, cpu_read(cpu, addr_abs(cpu)));
}

static void op_eor_abx(cpu6502_t *cpu) {
    do_eor(cpu, cpu_read(cpu, addr_abx(cpu)));
    if (cpu->page_crossed) cpu->cycles++;
}

static void op_eor_aby(cpu6502_t *cpu) {
    do_eor(cpu, cpu_read(cpu, addr_aby(cpu)));
    if (cpu->page_crossed) cpu->cycles++;
}

static void op_eor_izx(cpu6502_t *cpu) {
    do_eor(cpu, cpu_read(cpu, addr_izx(cpu)));
}

static void op_eor_izy(cpu6502_t *cpu) {
    do_eor(cpu, cpu_read(cpu, addr_izy(cpu)));
    if (cpu->page_crossed) cpu->cycles++;
}

/* ===== ORA ===== */

static void op_ora_imm(cpu6502_t *cpu) {
    do_ora(cpu, cpu_read(cpu, addr_imm(cpu)));
}

static void op_ora_zpg(cpu6502_t *cpu) {
    do_ora(cpu, cpu_read(cpu, addr_zpg(cpu)));
}

static void op_ora_zpx(cpu6502_t *cpu) {
    do_ora(cpu, cpu_read(cpu, addr_zpx(cpu)));
}

static void op_ora_abs(cpu6502_t *cpu) {
    do_ora(cpu, cpu_read(cpu, addr_abs(cpu)));
}

static void op_ora_abx(cpu6502_t *cpu) {
    do_ora(cpu, cpu_read(cpu, addr_abx(cpu)));
    if (cpu->page_crossed) cpu->cycles++;
}

static void op_ora_aby(cpu6502_t *cpu) {
    do_ora(cpu, cpu_read(cpu, addr_aby(cpu)));
    if (cpu->page_crossed) cpu->cycles++;
}

static void op_ora_izx(cpu6502_t *cpu) {
    do_ora(cpu, cpu_read(cpu, addr_izx(cpu)));
}

static void op_ora_izy(cpu6502_t *cpu) {
    do_ora(cpu, cpu_read(cpu, addr_izy(cpu)));
    if (cpu->page_crossed) cpu->cycles++;
}

/* ===== BIT ===== */

static void op_bit_zpg(cpu6502_t *cpu) {
    uint8_t val = cpu_read(cpu, addr_zpg(cpu));
    cpu_set_flag(cpu, CPU_FLAG_Z, (cpu->a & val) == 0);
    cpu_set_flag(cpu, CPU_FLAG_N, val & 0x80);
    cpu_set_flag(cpu, CPU_FLAG_V, val & 0x40);
}

static void op_bit_abs(cpu6502_t *cpu) {
    uint8_t val = cpu_read(cpu, addr_abs(cpu));
    cpu_set_flag(cpu, CPU_FLAG_Z, (cpu->a & val) == 0);
    cpu_set_flag(cpu, CPU_FLAG_N, val & 0x80);
    cpu_set_flag(cpu, CPU_FLAG_V, val & 0x40);
}

/* ===== ASL ===== */

static void op_asl_acc(cpu6502_t *cpu) {
    cpu->a = do_asl(cpu, cpu->a);
}

static void op_asl_zpg(cpu6502_t *cpu) {
    uint16_t addr = addr_zpg(cpu);
    uint8_t val = cpu_read(cpu, addr);
    val = do_asl(cpu, val);
    cpu_write(cpu, addr, val);
}

static void op_asl_zpx(cpu6502_t *cpu) {
    uint16_t addr = addr_zpx(cpu);
    uint8_t val = cpu_read(cpu, addr);
    val = do_asl(cpu, val);
    cpu_write(cpu, addr, val);
}

static void op_asl_abs(cpu6502_t *cpu) {
    uint16_t addr = addr_abs(cpu);
    uint8_t val = cpu_read(cpu, addr);
    val = do_asl(cpu, val);
    cpu_write(cpu, addr, val);
}

static void op_asl_abx(cpu6502_t *cpu) {
    uint16_t addr = addr_abx(cpu);
    uint8_t val = cpu_read(cpu, addr);
    val = do_asl(cpu, val);
    cpu_write(cpu, addr, val);
    /* RMW: no page crossing penalty (fixed cycle count) */
}

/* ===== LSR ===== */

static void op_lsr_acc(cpu6502_t *cpu) {
    cpu->a = do_lsr(cpu, cpu->a);
}

static void op_lsr_zpg(cpu6502_t *cpu) {
    uint16_t addr = addr_zpg(cpu);
    uint8_t val = cpu_read(cpu, addr);
    val = do_lsr(cpu, val);
    cpu_write(cpu, addr, val);
}

static void op_lsr_zpx(cpu6502_t *cpu) {
    uint16_t addr = addr_zpx(cpu);
    uint8_t val = cpu_read(cpu, addr);
    val = do_lsr(cpu, val);
    cpu_write(cpu, addr, val);
}

static void op_lsr_abs(cpu6502_t *cpu) {
    uint16_t addr = addr_abs(cpu);
    uint8_t val = cpu_read(cpu, addr);
    val = do_lsr(cpu, val);
    cpu_write(cpu, addr, val);
}

static void op_lsr_abx(cpu6502_t *cpu) {
    uint16_t addr = addr_abx(cpu);
    uint8_t val = cpu_read(cpu, addr);
    val = do_lsr(cpu, val);
    cpu_write(cpu, addr, val);
}

/* ===== ROL ===== */

static void op_rol_acc(cpu6502_t *cpu) {
    cpu->a = do_rol(cpu, cpu->a);
}

static void op_rol_zpg(cpu6502_t *cpu) {
    uint16_t addr = addr_zpg(cpu);
    uint8_t val = cpu_read(cpu, addr);
    val = do_rol(cpu, val);
    cpu_write(cpu, addr, val);
}

static void op_rol_zpx(cpu6502_t *cpu) {
    uint16_t addr = addr_zpx(cpu);
    uint8_t val = cpu_read(cpu, addr);
    val = do_rol(cpu, val);
    cpu_write(cpu, addr, val);
}

static void op_rol_abs(cpu6502_t *cpu) {
    uint16_t addr = addr_abs(cpu);
    uint8_t val = cpu_read(cpu, addr);
    val = do_rol(cpu, val);
    cpu_write(cpu, addr, val);
}

static void op_rol_abx(cpu6502_t *cpu) {
    uint16_t addr = addr_abx(cpu);
    uint8_t val = cpu_read(cpu, addr);
    val = do_rol(cpu, val);
    cpu_write(cpu, addr, val);
}

/* ===== ROR ===== */

static void op_ror_acc(cpu6502_t *cpu) {
    cpu->a = do_ror(cpu, cpu->a);
}

static void op_ror_zpg(cpu6502_t *cpu) {
    uint16_t addr = addr_zpg(cpu);
    uint8_t val = cpu_read(cpu, addr);
    val = do_ror(cpu, val);
    cpu_write(cpu, addr, val);
}

static void op_ror_zpx(cpu6502_t *cpu) {
    uint16_t addr = addr_zpx(cpu);
    uint8_t val = cpu_read(cpu, addr);
    val = do_ror(cpu, val);
    cpu_write(cpu, addr, val);
}

static void op_ror_abs(cpu6502_t *cpu) {
    uint16_t addr = addr_abs(cpu);
    uint8_t val = cpu_read(cpu, addr);
    val = do_ror(cpu, val);
    cpu_write(cpu, addr, val);
}

static void op_ror_abx(cpu6502_t *cpu) {
    uint16_t addr = addr_abx(cpu);
    uint8_t val = cpu_read(cpu, addr);
    val = do_ror(cpu, val);
    cpu_write(cpu, addr, val);
}

/* ===== INC ===== */

static void op_inc_zpg(cpu6502_t *cpu) {
    uint16_t addr = addr_zpg(cpu);
    uint8_t val = cpu_read(cpu, addr) + 1;
    cpu_write(cpu, addr, val);
    cpu_set_nz(cpu, val);
}

static void op_inc_zpx(cpu6502_t *cpu) {
    uint16_t addr = addr_zpx(cpu);
    uint8_t val = cpu_read(cpu, addr) + 1;
    cpu_write(cpu, addr, val);
    cpu_set_nz(cpu, val);
}

static void op_inc_abs(cpu6502_t *cpu) {
    uint16_t addr = addr_abs(cpu);
    uint8_t val = cpu_read(cpu, addr) + 1;
    cpu_write(cpu, addr, val);
    cpu_set_nz(cpu, val);
}

static void op_inc_abx(cpu6502_t *cpu) {
    uint16_t addr = addr_abx(cpu);
    uint8_t val = cpu_read(cpu, addr) + 1;
    cpu_write(cpu, addr, val);
    cpu_set_nz(cpu, val);
}

/* ===== DEC ===== */

static void op_dec_zpg(cpu6502_t *cpu) {
    uint16_t addr = addr_zpg(cpu);
    uint8_t val = cpu_read(cpu, addr) - 1;
    cpu_write(cpu, addr, val);
    cpu_set_nz(cpu, val);
}

static void op_dec_zpx(cpu6502_t *cpu) {
    uint16_t addr = addr_zpx(cpu);
    uint8_t val = cpu_read(cpu, addr) - 1;
    cpu_write(cpu, addr, val);
    cpu_set_nz(cpu, val);
}

static void op_dec_abs(cpu6502_t *cpu) {
    uint16_t addr = addr_abs(cpu);
    uint8_t val = cpu_read(cpu, addr) - 1;
    cpu_write(cpu, addr, val);
    cpu_set_nz(cpu, val);
}

static void op_dec_abx(cpu6502_t *cpu) {
    uint16_t addr = addr_abx(cpu);
    uint8_t val = cpu_read(cpu, addr) - 1;
    cpu_write(cpu, addr, val);
    cpu_set_nz(cpu, val);
}

/* ===== INX / INY / DEX / DEY ===== */

static void op_inx(cpu6502_t *cpu) {
    cpu->x++;
    cpu_set_nz(cpu, cpu->x);
}

static void op_iny(cpu6502_t *cpu) {
    cpu->y++;
    cpu_set_nz(cpu, cpu->y);
}

static void op_dex(cpu6502_t *cpu) {
    cpu->x--;
    cpu_set_nz(cpu, cpu->x);
}

static void op_dey(cpu6502_t *cpu) {
    cpu->y--;
    cpu_set_nz(cpu, cpu->y);
}

/* ===== Branch instructions ===== */

static void op_bpl(cpu6502_t *cpu) {
    int8_t offset = (int8_t)cpu_read(cpu, cpu->pc++);
    if (!cpu_get_flag(cpu, CPU_FLAG_N)) {
        uint16_t old_pc = cpu->pc;
        cpu->pc = (uint16_t)(cpu->pc + offset);
        cpu->cycles++;
        if ((old_pc & 0xFF00) != (cpu->pc & 0xFF00))
            cpu->cycles++;
    }
}

static void op_bmi(cpu6502_t *cpu) {
    int8_t offset = (int8_t)cpu_read(cpu, cpu->pc++);
    if (cpu_get_flag(cpu, CPU_FLAG_N)) {
        uint16_t old_pc = cpu->pc;
        cpu->pc = (uint16_t)(cpu->pc + offset);
        cpu->cycles++;
        if ((old_pc & 0xFF00) != (cpu->pc & 0xFF00))
            cpu->cycles++;
    }
}

static void op_bvc(cpu6502_t *cpu) {
    int8_t offset = (int8_t)cpu_read(cpu, cpu->pc++);
    if (!cpu_get_flag(cpu, CPU_FLAG_V)) {
        uint16_t old_pc = cpu->pc;
        cpu->pc = (uint16_t)(cpu->pc + offset);
        cpu->cycles++;
        if ((old_pc & 0xFF00) != (cpu->pc & 0xFF00))
            cpu->cycles++;
    }
}

static void op_bvs(cpu6502_t *cpu) {
    int8_t offset = (int8_t)cpu_read(cpu, cpu->pc++);
    if (cpu_get_flag(cpu, CPU_FLAG_V)) {
        uint16_t old_pc = cpu->pc;
        cpu->pc = (uint16_t)(cpu->pc + offset);
        cpu->cycles++;
        if ((old_pc & 0xFF00) != (cpu->pc & 0xFF00))
            cpu->cycles++;
    }
}

static void op_bcc(cpu6502_t *cpu) {
    int8_t offset = (int8_t)cpu_read(cpu, cpu->pc++);
    if (!cpu_get_flag(cpu, CPU_FLAG_C)) {
        uint16_t old_pc = cpu->pc;
        cpu->pc = (uint16_t)(cpu->pc + offset);
        cpu->cycles++;
        if ((old_pc & 0xFF00) != (cpu->pc & 0xFF00))
            cpu->cycles++;
    }
}

static void op_bcs(cpu6502_t *cpu) {
    int8_t offset = (int8_t)cpu_read(cpu, cpu->pc++);
    if (cpu_get_flag(cpu, CPU_FLAG_C)) {
        uint16_t old_pc = cpu->pc;
        cpu->pc = (uint16_t)(cpu->pc + offset);
        cpu->cycles++;
        if ((old_pc & 0xFF00) != (cpu->pc & 0xFF00))
            cpu->cycles++;
    }
}

static void op_bne(cpu6502_t *cpu) {
    int8_t offset = (int8_t)cpu_read(cpu, cpu->pc++);
    if (!cpu_get_flag(cpu, CPU_FLAG_Z)) {
        uint16_t old_pc = cpu->pc;
        cpu->pc = (uint16_t)(cpu->pc + offset);
        cpu->cycles++;
        if ((old_pc & 0xFF00) != (cpu->pc & 0xFF00))
            cpu->cycles++;
    }
}

static void op_beq(cpu6502_t *cpu) {
    int8_t offset = (int8_t)cpu_read(cpu, cpu->pc++);
    if (cpu_get_flag(cpu, CPU_FLAG_Z)) {
        uint16_t old_pc = cpu->pc;
        cpu->pc = (uint16_t)(cpu->pc + offset);
        cpu->cycles++;
        if ((old_pc & 0xFF00) != (cpu->pc & 0xFF00))
            cpu->cycles++;
    }
}

/* ===== JMP ===== */

static void op_jmp_abs(cpu6502_t *cpu) {
    cpu->pc = addr_abs(cpu);
}

/* JMP indirect with NMOS page boundary bug */
static void op_jmp_ind(cpu6502_t *cpu) {
    uint16_t ptr = addr_abs(cpu);
    uint16_t lo = cpu_read(cpu, ptr);
    /* Bug: high byte wraps within same page instead of crossing */
    uint16_t hi = cpu_read(cpu, (ptr & 0xFF00) | ((ptr + 1) & 0x00FF));
    cpu->pc = (hi << 8) | lo;
}

/* ===== JSR / RTS ===== */

static void op_jsr(cpu6502_t *cpu) {
    uint16_t addr = addr_abs(cpu);
    cpu_push16(cpu, cpu->pc - 1);  /* push return address - 1 */
    cpu->pc = addr;
}

static void op_rts(cpu6502_t *cpu) {
    cpu->pc = cpu_pull16(cpu) + 1;
}

/* ===== BRK / RTI ===== */

static void op_brk(cpu6502_t *cpu) {
    cpu->pc++;  /* skip padding byte */
    cpu_push16(cpu, cpu->pc);
    cpu_push(cpu, cpu->status | CPU_FLAG_B | CPU_FLAG_U);
    cpu_set_flag(cpu, CPU_FLAG_I, true);
    cpu->pc = cpu_read(cpu, 0xFFFE) | (cpu_read(cpu, 0xFFFF) << 8);
}

static void op_rti(cpu6502_t *cpu) {
    cpu->status = cpu_pull(cpu);
    cpu->status |= CPU_FLAG_U;    /* bit 5 always 1 */
    cpu->status &= ~CPU_FLAG_B;   /* B is not a real flag */
    cpu->pc = cpu_pull16(cpu);     /* no +1 unlike RTS */
}

/* ===== Stack ===== */

static void op_pha(cpu6502_t *cpu) {
    cpu_push(cpu, cpu->a);
}

static void op_pla(cpu6502_t *cpu) {
    cpu->a = cpu_pull(cpu);
    cpu_set_nz(cpu, cpu->a);
}

static void op_php(cpu6502_t *cpu) {
    cpu_push(cpu, cpu->status | CPU_FLAG_B | CPU_FLAG_U);
}

static void op_plp(cpu6502_t *cpu) {
    cpu->status = cpu_pull(cpu);
    cpu->status |= CPU_FLAG_U;
    cpu->status &= ~CPU_FLAG_B;
}

/* ===== Transfers ===== */

static void op_tax(cpu6502_t *cpu) {
    cpu->x = cpu->a;
    cpu_set_nz(cpu, cpu->x);
}

static void op_tay(cpu6502_t *cpu) {
    cpu->y = cpu->a;
    cpu_set_nz(cpu, cpu->y);
}

static void op_txa(cpu6502_t *cpu) {
    cpu->a = cpu->x;
    cpu_set_nz(cpu, cpu->a);
}

static void op_tya(cpu6502_t *cpu) {
    cpu->a = cpu->y;
    cpu_set_nz(cpu, cpu->a);
}

static void op_tsx(cpu6502_t *cpu) {
    cpu->x = cpu->sp;
    cpu_set_nz(cpu, cpu->x);
}

/* TXS does NOT set any flags */
static void op_txs(cpu6502_t *cpu) {
    cpu->sp = cpu->x;
}

/* ===== Flag instructions ===== */

static void op_clc(cpu6502_t *cpu) {
    cpu_set_flag(cpu, CPU_FLAG_C, false);
}

static void op_sec(cpu6502_t *cpu) {
    cpu_set_flag(cpu, CPU_FLAG_C, true);
}

static void op_cli(cpu6502_t *cpu) {
    cpu_set_flag(cpu, CPU_FLAG_I, false);
}

static void op_sei(cpu6502_t *cpu) {
    cpu_set_flag(cpu, CPU_FLAG_I, true);
}

static void op_cld(cpu6502_t *cpu) {
    cpu_set_flag(cpu, CPU_FLAG_D, false);
}

static void op_sed(cpu6502_t *cpu) {
    cpu_set_flag(cpu, CPU_FLAG_D, true);
}

static void op_clv(cpu6502_t *cpu) {
    cpu_set_flag(cpu, CPU_FLAG_V, false);
}

/* ===== NOP ===== */

static void op_nop(cpu6502_t *cpu) {
    (void)cpu;
}

/* ======================================================================
 * 4. Exported tables
 * ====================================================================== */

const opcode_fn opcode_table[256] = {
    /* 0x00-0x0F */
    [0x00] = op_brk,     [0x01] = op_ora_izx, [0x02] = op_ill,     [0x03] = op_ill,
    [0x04] = op_ill,     [0x05] = op_ora_zpg, [0x06] = op_asl_zpg, [0x07] = op_ill,
    [0x08] = op_php,     [0x09] = op_ora_imm, [0x0A] = op_asl_acc, [0x0B] = op_ill,
    [0x0C] = op_ill,     [0x0D] = op_ora_abs, [0x0E] = op_asl_abs, [0x0F] = op_ill,

    /* 0x10-0x1F */
    [0x10] = op_bpl,     [0x11] = op_ora_izy, [0x12] = op_ill,     [0x13] = op_ill,
    [0x14] = op_ill,     [0x15] = op_ora_zpx, [0x16] = op_asl_zpx, [0x17] = op_ill,
    [0x18] = op_clc,     [0x19] = op_ora_aby, [0x1A] = op_ill,     [0x1B] = op_ill,
    [0x1C] = op_ill,     [0x1D] = op_ora_abx, [0x1E] = op_asl_abx, [0x1F] = op_ill,

    /* 0x20-0x2F */
    [0x20] = op_jsr,     [0x21] = op_and_izx, [0x22] = op_ill,     [0x23] = op_ill,
    [0x24] = op_bit_zpg, [0x25] = op_and_zpg, [0x26] = op_rol_zpg, [0x27] = op_ill,
    [0x28] = op_plp,     [0x29] = op_and_imm, [0x2A] = op_rol_acc, [0x2B] = op_ill,
    [0x2C] = op_bit_abs, [0x2D] = op_and_abs, [0x2E] = op_rol_abs, [0x2F] = op_ill,

    /* 0x30-0x3F */
    [0x30] = op_bmi,     [0x31] = op_and_izy, [0x32] = op_ill,     [0x33] = op_ill,
    [0x34] = op_ill,     [0x35] = op_and_zpx, [0x36] = op_rol_zpx, [0x37] = op_ill,
    [0x38] = op_sec,     [0x39] = op_and_aby, [0x3A] = op_ill,     [0x3B] = op_ill,
    [0x3C] = op_ill,     [0x3D] = op_and_abx, [0x3E] = op_rol_abx, [0x3F] = op_ill,

    /* 0x40-0x4F */
    [0x40] = op_rti,     [0x41] = op_eor_izx, [0x42] = op_ill,     [0x43] = op_ill,
    [0x44] = op_ill,     [0x45] = op_eor_zpg, [0x46] = op_lsr_zpg, [0x47] = op_ill,
    [0x48] = op_pha,     [0x49] = op_eor_imm, [0x4A] = op_lsr_acc, [0x4B] = op_ill,
    [0x4C] = op_jmp_abs, [0x4D] = op_eor_abs, [0x4E] = op_lsr_abs, [0x4F] = op_ill,

    /* 0x50-0x5F */
    [0x50] = op_bvc,     [0x51] = op_eor_izy, [0x52] = op_ill,     [0x53] = op_ill,
    [0x54] = op_ill,     [0x55] = op_eor_zpx, [0x56] = op_lsr_zpx, [0x57] = op_ill,
    [0x58] = op_cli,     [0x59] = op_eor_aby, [0x5A] = op_ill,     [0x5B] = op_ill,
    [0x5C] = op_ill,     [0x5D] = op_eor_abx, [0x5E] = op_lsr_abx, [0x5F] = op_ill,

    /* 0x60-0x6F */
    [0x60] = op_rts,     [0x61] = op_adc_izx, [0x62] = op_ill,     [0x63] = op_ill,
    [0x64] = op_ill,     [0x65] = op_adc_zpg, [0x66] = op_ror_zpg, [0x67] = op_ill,
    [0x68] = op_pla,     [0x69] = op_adc_imm, [0x6A] = op_ror_acc, [0x6B] = op_ill,
    [0x6C] = op_jmp_ind, [0x6D] = op_adc_abs, [0x6E] = op_ror_abs, [0x6F] = op_ill,

    /* 0x70-0x7F */
    [0x70] = op_bvs,     [0x71] = op_adc_izy, [0x72] = op_ill,     [0x73] = op_ill,
    [0x74] = op_ill,     [0x75] = op_adc_zpx, [0x76] = op_ror_zpx, [0x77] = op_ill,
    [0x78] = op_sei,     [0x79] = op_adc_aby, [0x7A] = op_ill,     [0x7B] = op_ill,
    [0x7C] = op_ill,     [0x7D] = op_adc_abx, [0x7E] = op_ror_abx, [0x7F] = op_ill,

    /* 0x80-0x8F */
    [0x80] = op_ill,     [0x81] = op_sta_izx, [0x82] = op_ill,     [0x83] = op_ill,
    [0x84] = op_sty_zpg, [0x85] = op_sta_zpg, [0x86] = op_stx_zpg, [0x87] = op_ill,
    [0x88] = op_dey,     [0x89] = op_ill,     [0x8A] = op_txa,     [0x8B] = op_ill,
    [0x8C] = op_sty_abs, [0x8D] = op_sta_abs, [0x8E] = op_stx_abs, [0x8F] = op_ill,

    /* 0x90-0x9F */
    [0x90] = op_bcc,     [0x91] = op_sta_izy, [0x92] = op_ill,     [0x93] = op_ill,
    [0x94] = op_sty_zpx, [0x95] = op_sta_zpx, [0x96] = op_stx_zpy, [0x97] = op_ill,
    [0x98] = op_tya,     [0x99] = op_sta_aby, [0x9A] = op_txs,     [0x9B] = op_ill,
    [0x9C] = op_ill,     [0x9D] = op_sta_abx, [0x9E] = op_ill,     [0x9F] = op_ill,

    /* 0xA0-0xAF */
    [0xA0] = op_ldy_imm, [0xA1] = op_lda_izx, [0xA2] = op_ldx_imm, [0xA3] = op_ill,
    [0xA4] = op_ldy_zpg, [0xA5] = op_lda_zpg, [0xA6] = op_ldx_zpg, [0xA7] = op_ill,
    [0xA8] = op_tay,     [0xA9] = op_lda_imm, [0xAA] = op_tax,     [0xAB] = op_ill,
    [0xAC] = op_ldy_abs, [0xAD] = op_lda_abs, [0xAE] = op_ldx_abs, [0xAF] = op_ill,

    /* 0xB0-0xBF */
    [0xB0] = op_bcs,     [0xB1] = op_lda_izy, [0xB2] = op_ill,     [0xB3] = op_ill,
    [0xB4] = op_ldy_zpx, [0xB5] = op_lda_zpx, [0xB6] = op_ldx_zpy, [0xB7] = op_ill,
    [0xB8] = op_clv,     [0xB9] = op_lda_aby, [0xBA] = op_tsx,     [0xBB] = op_ill,
    [0xBC] = op_ldy_abx, [0xBD] = op_lda_abx, [0xBE] = op_ldx_aby, [0xBF] = op_ill,

    /* 0xC0-0xCF */
    [0xC0] = op_cpy_imm, [0xC1] = op_cmp_izx, [0xC2] = op_ill,     [0xC3] = op_ill,
    [0xC4] = op_cpy_zpg, [0xC5] = op_cmp_zpg, [0xC6] = op_dec_zpg, [0xC7] = op_ill,
    [0xC8] = op_iny,     [0xC9] = op_cmp_imm, [0xCA] = op_dex,     [0xCB] = op_ill,
    [0xCC] = op_cpy_abs, [0xCD] = op_cmp_abs, [0xCE] = op_dec_abs, [0xCF] = op_ill,

    /* 0xD0-0xDF */
    [0xD0] = op_bne,     [0xD1] = op_cmp_izy, [0xD2] = op_ill,     [0xD3] = op_ill,
    [0xD4] = op_ill,     [0xD5] = op_cmp_zpx, [0xD6] = op_dec_zpx, [0xD7] = op_ill,
    [0xD8] = op_cld,     [0xD9] = op_cmp_aby, [0xDA] = op_ill,     [0xDB] = op_ill,
    [0xDC] = op_ill,     [0xDD] = op_cmp_abx, [0xDE] = op_dec_abx, [0xDF] = op_ill,

    /* 0xE0-0xEF */
    [0xE0] = op_cpx_imm, [0xE1] = op_sbc_izx, [0xE2] = op_ill,     [0xE3] = op_ill,
    [0xE4] = op_cpx_zpg, [0xE5] = op_sbc_zpg, [0xE6] = op_inc_zpg, [0xE7] = op_ill,
    [0xE8] = op_inx,     [0xE9] = op_sbc_imm, [0xEA] = op_nop,     [0xEB] = op_ill,
    [0xEC] = op_cpx_abs, [0xED] = op_sbc_abs, [0xEE] = op_inc_abs, [0xEF] = op_ill,

    /* 0xF0-0xFF */
    [0xF0] = op_beq,     [0xF1] = op_sbc_izy, [0xF2] = op_ill,     [0xF3] = op_ill,
    [0xF4] = op_ill,     [0xF5] = op_sbc_zpx, [0xF6] = op_inc_zpx, [0xF7] = op_ill,
    [0xF8] = op_sed,     [0xF9] = op_sbc_aby, [0xFA] = op_ill,     [0xFB] = op_ill,
    [0xFC] = op_ill,     [0xFD] = op_sbc_abx, [0xFE] = op_inc_abx, [0xFF] = op_ill,
};

const uint8_t opcode_cycles[256] = {
    /* 0x00-0x0F */  7,6,0,0,0,3,5,0,3,2,2,0,0,4,6,0,
    /* 0x10-0x1F */  2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,
    /* 0x20-0x2F */  6,6,0,0,3,3,5,0,4,2,2,0,4,4,6,0,
    /* 0x30-0x3F */  2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,
    /* 0x40-0x4F */  6,6,0,0,0,3,5,0,3,2,2,0,3,4,6,0,
    /* 0x50-0x5F */  2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,
    /* 0x60-0x6F */  6,6,0,0,0,3,5,0,4,2,2,0,5,4,6,0,
    /* 0x70-0x7F */  2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,
    /* 0x80-0x8F */  0,6,0,0,3,3,3,0,2,0,2,0,4,4,4,0,
    /* 0x90-0x9F */  2,6,0,0,4,4,4,0,2,5,2,0,0,5,0,0,
    /* 0xA0-0xAF */  2,6,2,0,3,3,3,0,2,2,2,0,4,4,4,0,
    /* 0xB0-0xBF */  2,5,0,0,4,4,4,0,2,4,2,0,4,4,4,0,
    /* 0xC0-0xCF */  2,6,0,0,3,3,5,0,2,2,2,0,4,4,6,0,
    /* 0xD0-0xDF */  2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,
    /* 0xE0-0xEF */  2,6,0,0,3,3,5,0,2,2,2,0,4,4,6,0,
    /* 0xF0-0xFF */  2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,
};

const char *opcode_names[256] = {
    /* 0x00 */ "BRK", "ORA", "???", "???", "???", "ORA", "ASL", "???",
    /* 0x08 */ "PHP", "ORA", "ASL", "???", "???", "ORA", "ASL", "???",
    /* 0x10 */ "BPL", "ORA", "???", "???", "???", "ORA", "ASL", "???",
    /* 0x18 */ "CLC", "ORA", "???", "???", "???", "ORA", "ASL", "???",
    /* 0x20 */ "JSR", "AND", "???", "???", "BIT", "AND", "ROL", "???",
    /* 0x28 */ "PLP", "AND", "ROL", "???", "BIT", "AND", "ROL", "???",
    /* 0x30 */ "BMI", "AND", "???", "???", "???", "AND", "ROL", "???",
    /* 0x38 */ "SEC", "AND", "???", "???", "???", "AND", "ROL", "???",
    /* 0x40 */ "RTI", "EOR", "???", "???", "???", "EOR", "LSR", "???",
    /* 0x48 */ "PHA", "EOR", "LSR", "???", "JMP", "EOR", "LSR", "???",
    /* 0x50 */ "BVC", "EOR", "???", "???", "???", "EOR", "LSR", "???",
    /* 0x58 */ "CLI", "EOR", "???", "???", "???", "EOR", "LSR", "???",
    /* 0x60 */ "RTS", "ADC", "???", "???", "???", "ADC", "ROR", "???",
    /* 0x68 */ "PLA", "ADC", "ROR", "???", "JMP", "ADC", "ROR", "???",
    /* 0x70 */ "BVS", "ADC", "???", "???", "???", "ADC", "ROR", "???",
    /* 0x78 */ "SEI", "ADC", "???", "???", "???", "ADC", "ROR", "???",
    /* 0x80 */ "???", "STA", "???", "???", "STY", "STA", "STX", "???",
    /* 0x88 */ "DEY", "???", "TXA", "???", "STY", "STA", "STX", "???",
    /* 0x90 */ "BCC", "STA", "???", "???", "STY", "STA", "STX", "???",
    /* 0x98 */ "TYA", "STA", "TXS", "???", "???", "STA", "???", "???",
    /* 0xA0 */ "LDY", "LDA", "LDX", "???", "LDY", "LDA", "LDX", "???",
    /* 0xA8 */ "TAY", "LDA", "TAX", "???", "LDY", "LDA", "LDX", "???",
    /* 0xB0 */ "BCS", "LDA", "???", "???", "LDY", "LDA", "LDX", "???",
    /* 0xB8 */ "CLV", "LDA", "TSX", "???", "LDY", "LDA", "LDX", "???",
    /* 0xC0 */ "CPY", "CMP", "???", "???", "CPY", "CMP", "DEC", "???",
    /* 0xC8 */ "INY", "CMP", "DEX", "???", "CPY", "CMP", "DEC", "???",
    /* 0xD0 */ "BNE", "CMP", "???", "???", "???", "CMP", "DEC", "???",
    /* 0xD8 */ "CLD", "CMP", "???", "???", "???", "CMP", "DEC", "???",
    /* 0xE0 */ "CPX", "SBC", "???", "???", "CPX", "SBC", "INC", "???",
    /* 0xE8 */ "INX", "SBC", "NOP", "???", "CPX", "SBC", "INC", "???",
    /* 0xF0 */ "BEQ", "SBC", "???", "???", "???", "SBC", "INC", "???",
    /* 0xF8 */ "SED", "SBC", "???", "???", "???", "SBC", "INC", "???",
};
