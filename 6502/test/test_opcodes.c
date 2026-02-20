#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../src/bus.h"
#include "../src/cpu6502.h"

/* ------------------------------------------------------------------ */
/*  Helpers                                                           */
/* ------------------------------------------------------------------ */

#define ASSERT(cond, ...) \
    do { if (!(cond)) { fprintf(stderr, "FAIL: " __VA_ARGS__); return 1; } } while (0)

/* Initialize bus+cpu with reset vector pointing to $0600 */
static void setup(bus_flat_t *bus, cpu6502_t *cpu)
{
    bus_flat_init(bus);
    bus->ram[0xFFFC] = 0x00;  /* reset vector low  -> $0600 */
    bus->ram[0xFFFD] = 0x06;  /* reset vector high */
    cpu6502_init(cpu, bus_flat_read, bus_flat_write, bus);
    cpu6502_reset(cpu);
}

/* ================================================================== */
/*  Load / Store                                                      */
/* ================================================================== */

int test_lda_imm_basic(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA9;   /* LDA #$42 */
    bus.ram[0x0601] = 0x42;
    cpu6502_step(&cpu);
    ASSERT(cpu.a == 0x42,       "test_lda_imm_basic: A=%02X expected 42\n", cpu.a);
    ASSERT(!cpu_get_flag(&cpu, CPU_FLAG_Z), "test_lda_imm_basic: Z set\n");
    ASSERT(!cpu_get_flag(&cpu, CPU_FLAG_N), "test_lda_imm_basic: N set\n");
    return 0;
}

int test_lda_imm_zero(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA9;   /* LDA #$00 */
    bus.ram[0x0601] = 0x00;
    cpu6502_step(&cpu);
    ASSERT(cpu.a == 0x00,       "test_lda_imm_zero: A=%02X expected 00\n", cpu.a);
    ASSERT(cpu_get_flag(&cpu, CPU_FLAG_Z),  "test_lda_imm_zero: Z not set\n");
    ASSERT(!cpu_get_flag(&cpu, CPU_FLAG_N), "test_lda_imm_zero: N set\n");
    return 0;
}

int test_lda_imm_negative(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA9;   /* LDA #$80 */
    bus.ram[0x0601] = 0x80;
    cpu6502_step(&cpu);
    ASSERT(cpu.a == 0x80,       "test_lda_imm_negative: A=%02X expected 80\n", cpu.a);
    ASSERT(!cpu_get_flag(&cpu, CPU_FLAG_Z), "test_lda_imm_negative: Z set\n");
    ASSERT(cpu_get_flag(&cpu, CPU_FLAG_N),  "test_lda_imm_negative: N not set\n");
    return 0;
}

int test_lda_zpg(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0010] = 0x42;   /* value at zero-page $10 */
    bus.ram[0x0600] = 0xA5;   /* LDA $10 */
    bus.ram[0x0601] = 0x10;
    cpu6502_step(&cpu);
    ASSERT(cpu.a == 0x42, "test_lda_zpg: A=%02X expected 42\n", cpu.a);
    return 0;
}

int test_lda_abs(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x1234] = 0x42;   /* value at $1234 */
    bus.ram[0x0600] = 0xAD;   /* LDA $1234 */
    bus.ram[0x0601] = 0x34;   /* low byte */
    bus.ram[0x0602] = 0x12;   /* high byte */
    cpu6502_step(&cpu);
    ASSERT(cpu.a == 0x42, "test_lda_abs: A=%02X expected 42\n", cpu.a);
    return 0;
}

int test_lda_abx_page_cross(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    /* Set X = 1 first via LDX #$01 */
    bus.ram[0x0600] = 0xA2;   /* LDX #$01 */
    bus.ram[0x0601] = 0x01;
    cpu6502_step(&cpu);

    uint64_t cycles_before = cpu.cycles;

    /* LDA $10FF,X -- effective address $1100, crosses page */
    bus.ram[0x1100] = 0x42;   /* value at effective address */
    bus.ram[0x0602] = 0xBD;   /* LDA abs,X */
    bus.ram[0x0603] = 0xFF;   /* low byte */
    bus.ram[0x0604] = 0x10;   /* high byte */
    cpu6502_step(&cpu);

    uint64_t elapsed = cpu.cycles - cycles_before;
    ASSERT(cpu.a == 0x42, "test_lda_abx_page_cross: A=%02X expected 42\n", cpu.a);
    /* LDA abs,X base = 4 cycles, +1 for page cross = 5 */
    ASSERT(elapsed == 5, "test_lda_abx_page_cross: cycles=%llu expected 5\n",
           (unsigned long long)elapsed);
    return 0;
}

int test_sta_zpg(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA9;   /* LDA #$42 */
    bus.ram[0x0601] = 0x42;
    bus.ram[0x0602] = 0x85;   /* STA $10 */
    bus.ram[0x0603] = 0x10;
    cpu6502_step(&cpu);
    cpu6502_step(&cpu);
    ASSERT(bus.ram[0x0010] == 0x42,
           "test_sta_zpg: mem[$10]=%02X expected 42\n", bus.ram[0x0010]);
    return 0;
}

int test_sta_abs(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA9;   /* LDA #$42 */
    bus.ram[0x0601] = 0x42;
    bus.ram[0x0602] = 0x8D;   /* STA $1234 */
    bus.ram[0x0603] = 0x34;
    bus.ram[0x0604] = 0x12;
    cpu6502_step(&cpu);
    cpu6502_step(&cpu);
    ASSERT(bus.ram[0x1234] == 0x42,
           "test_sta_abs: mem[$1234]=%02X expected 42\n", bus.ram[0x1234]);
    return 0;
}

int test_ldx_imm(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA2;   /* LDX #$42 */
    bus.ram[0x0601] = 0x42;
    cpu6502_step(&cpu);
    ASSERT(cpu.x == 0x42, "test_ldx_imm: X=%02X expected 42\n", cpu.x);
    return 0;
}

int test_ldy_imm(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA0;   /* LDY #$42 */
    bus.ram[0x0601] = 0x42;
    cpu6502_step(&cpu);
    ASSERT(cpu.y == 0x42, "test_ldy_imm: Y=%02X expected 42\n", cpu.y);
    return 0;
}

/* ================================================================== */
/*  Arithmetic                                                        */
/* ================================================================== */

int test_adc_no_carry(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA9;   /* LDA #$10 */
    bus.ram[0x0601] = 0x10;
    bus.ram[0x0602] = 0x18;   /* CLC */
    bus.ram[0x0603] = 0x69;   /* ADC #$20 */
    bus.ram[0x0604] = 0x20;
    cpu6502_step(&cpu);       /* LDA */
    cpu6502_step(&cpu);       /* CLC */
    cpu6502_step(&cpu);       /* ADC */
    ASSERT(cpu.a == 0x30,       "test_adc_no_carry: A=%02X expected 30\n", cpu.a);
    ASSERT(!cpu_get_flag(&cpu, CPU_FLAG_C), "test_adc_no_carry: C set\n");
    ASSERT(!cpu_get_flag(&cpu, CPU_FLAG_V), "test_adc_no_carry: V set\n");
    ASSERT(!cpu_get_flag(&cpu, CPU_FLAG_Z), "test_adc_no_carry: Z set\n");
    ASSERT(!cpu_get_flag(&cpu, CPU_FLAG_N), "test_adc_no_carry: N set\n");
    return 0;
}

int test_adc_with_carry_in(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0x38;   /* SEC */
    bus.ram[0x0601] = 0xA9;   /* LDA #$10 */
    bus.ram[0x0602] = 0x10;
    bus.ram[0x0603] = 0x69;   /* ADC #$20 */
    bus.ram[0x0604] = 0x20;
    cpu6502_step(&cpu);       /* SEC */
    cpu6502_step(&cpu);       /* LDA */
    cpu6502_step(&cpu);       /* ADC */
    ASSERT(cpu.a == 0x31,       "test_adc_with_carry_in: A=%02X expected 31\n", cpu.a);
    ASSERT(!cpu_get_flag(&cpu, CPU_FLAG_C), "test_adc_with_carry_in: C set\n");
    return 0;
}

int test_adc_carry_out(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA9;   /* LDA #$FF */
    bus.ram[0x0601] = 0xFF;
    bus.ram[0x0602] = 0x18;   /* CLC */
    bus.ram[0x0603] = 0x69;   /* ADC #$01 */
    bus.ram[0x0604] = 0x01;
    cpu6502_step(&cpu);       /* LDA */
    cpu6502_step(&cpu);       /* CLC */
    cpu6502_step(&cpu);       /* ADC */
    ASSERT(cpu.a == 0x00,       "test_adc_carry_out: A=%02X expected 00\n", cpu.a);
    ASSERT(cpu_get_flag(&cpu, CPU_FLAG_C),  "test_adc_carry_out: C not set\n");
    ASSERT(cpu_get_flag(&cpu, CPU_FLAG_Z),  "test_adc_carry_out: Z not set\n");
    return 0;
}

int test_adc_overflow_pos(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA9;   /* LDA #$7F */
    bus.ram[0x0601] = 0x7F;
    bus.ram[0x0602] = 0x18;   /* CLC */
    bus.ram[0x0603] = 0x69;   /* ADC #$01 */
    bus.ram[0x0604] = 0x01;
    cpu6502_step(&cpu);       /* LDA */
    cpu6502_step(&cpu);       /* CLC */
    cpu6502_step(&cpu);       /* ADC */
    ASSERT(cpu.a == 0x80,       "test_adc_overflow_pos: A=%02X expected 80\n", cpu.a);
    ASSERT(cpu_get_flag(&cpu, CPU_FLAG_V),  "test_adc_overflow_pos: V not set\n");
    ASSERT(cpu_get_flag(&cpu, CPU_FLAG_N),  "test_adc_overflow_pos: N not set\n");
    return 0;
}

int test_sbc_basic(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0x38;   /* SEC (no borrow) */
    bus.ram[0x0601] = 0xA9;   /* LDA #$30 */
    bus.ram[0x0602] = 0x30;
    bus.ram[0x0603] = 0xE9;   /* SBC #$10 */
    bus.ram[0x0604] = 0x10;
    cpu6502_step(&cpu);       /* SEC */
    cpu6502_step(&cpu);       /* LDA */
    cpu6502_step(&cpu);       /* SBC */
    ASSERT(cpu.a == 0x20,       "test_sbc_basic: A=%02X expected 20\n", cpu.a);
    ASSERT(cpu_get_flag(&cpu, CPU_FLAG_C),  "test_sbc_basic: C not set (no borrow)\n");
    return 0;
}

int test_sbc_borrow(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0x18;   /* CLC (borrow active) */
    bus.ram[0x0601] = 0xA9;   /* LDA #$30 */
    bus.ram[0x0602] = 0x30;
    bus.ram[0x0603] = 0xE9;   /* SBC #$10 */
    bus.ram[0x0604] = 0x10;
    cpu6502_step(&cpu);       /* CLC */
    cpu6502_step(&cpu);       /* LDA */
    cpu6502_step(&cpu);       /* SBC */
    /* SBC: A = A - M - (1-C) = $30 - $10 - 1 = $1F */
    ASSERT(cpu.a == 0x1F,       "test_sbc_borrow: A=%02X expected 1F\n", cpu.a);
    return 0;
}

/* ================================================================== */
/*  Compare                                                           */
/* ================================================================== */

int test_cmp_equal(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA9;   /* LDA #$42 */
    bus.ram[0x0601] = 0x42;
    bus.ram[0x0602] = 0xC9;   /* CMP #$42 */
    bus.ram[0x0603] = 0x42;
    cpu6502_step(&cpu);       /* LDA */
    cpu6502_step(&cpu);       /* CMP */
    ASSERT(cpu_get_flag(&cpu, CPU_FLAG_Z),  "test_cmp_equal: Z not set\n");
    ASSERT(cpu_get_flag(&cpu, CPU_FLAG_C),  "test_cmp_equal: C not set\n");
    ASSERT(!cpu_get_flag(&cpu, CPU_FLAG_N), "test_cmp_equal: N set\n");
    return 0;
}

int test_cmp_greater(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA9;   /* LDA #$42 */
    bus.ram[0x0601] = 0x42;
    bus.ram[0x0602] = 0xC9;   /* CMP #$10 */
    bus.ram[0x0603] = 0x10;
    cpu6502_step(&cpu);       /* LDA */
    cpu6502_step(&cpu);       /* CMP */
    ASSERT(!cpu_get_flag(&cpu, CPU_FLAG_Z), "test_cmp_greater: Z set\n");
    ASSERT(cpu_get_flag(&cpu, CPU_FLAG_C),  "test_cmp_greater: C not set\n");
    return 0;
}

int test_cmp_less(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA9;   /* LDA #$10 */
    bus.ram[0x0601] = 0x10;
    bus.ram[0x0602] = 0xC9;   /* CMP #$42 */
    bus.ram[0x0603] = 0x42;
    cpu6502_step(&cpu);       /* LDA */
    cpu6502_step(&cpu);       /* CMP */
    ASSERT(!cpu_get_flag(&cpu, CPU_FLAG_Z), "test_cmp_less: Z set\n");
    ASSERT(!cpu_get_flag(&cpu, CPU_FLAG_C), "test_cmp_less: C set\n");
    return 0;
}

/* ================================================================== */
/*  Logical                                                           */
/* ================================================================== */

int test_and_basic(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA9;   /* LDA #$FF */
    bus.ram[0x0601] = 0xFF;
    bus.ram[0x0602] = 0x29;   /* AND #$0F */
    bus.ram[0x0603] = 0x0F;
    cpu6502_step(&cpu);       /* LDA */
    cpu6502_step(&cpu);       /* AND */
    ASSERT(cpu.a == 0x0F, "test_and_basic: A=%02X expected 0F\n", cpu.a);
    return 0;
}

int test_ora_basic(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA9;   /* LDA #$F0 */
    bus.ram[0x0601] = 0xF0;
    bus.ram[0x0602] = 0x09;   /* ORA #$0F */
    bus.ram[0x0603] = 0x0F;
    cpu6502_step(&cpu);       /* LDA */
    cpu6502_step(&cpu);       /* ORA */
    ASSERT(cpu.a == 0xFF, "test_ora_basic: A=%02X expected FF\n", cpu.a);
    return 0;
}

int test_eor_basic(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA9;   /* LDA #$FF */
    bus.ram[0x0601] = 0xFF;
    bus.ram[0x0602] = 0x49;   /* EOR #$0F */
    bus.ram[0x0603] = 0x0F;
    cpu6502_step(&cpu);       /* LDA */
    cpu6502_step(&cpu);       /* EOR */
    ASSERT(cpu.a == 0xF0, "test_eor_basic: A=%02X expected F0\n", cpu.a);
    return 0;
}

/* ================================================================== */
/*  Shifts                                                            */
/* ================================================================== */

int test_asl_acc(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA9;   /* LDA #$80 */
    bus.ram[0x0601] = 0x80;
    bus.ram[0x0602] = 0x0A;   /* ASL A */
    cpu6502_step(&cpu);       /* LDA */
    cpu6502_step(&cpu);       /* ASL A */
    ASSERT(cpu.a == 0x00,       "test_asl_acc: A=%02X expected 00\n", cpu.a);
    ASSERT(cpu_get_flag(&cpu, CPU_FLAG_C),  "test_asl_acc: C not set\n");
    ASSERT(cpu_get_flag(&cpu, CPU_FLAG_Z),  "test_asl_acc: Z not set\n");
    return 0;
}

int test_lsr_acc(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA9;   /* LDA #$01 */
    bus.ram[0x0601] = 0x01;
    bus.ram[0x0602] = 0x4A;   /* LSR A */
    cpu6502_step(&cpu);       /* LDA */
    cpu6502_step(&cpu);       /* LSR A */
    ASSERT(cpu.a == 0x00,       "test_lsr_acc: A=%02X expected 00\n", cpu.a);
    ASSERT(cpu_get_flag(&cpu, CPU_FLAG_C),  "test_lsr_acc: C not set\n");
    ASSERT(cpu_get_flag(&cpu, CPU_FLAG_Z),  "test_lsr_acc: Z not set\n");
    return 0;
}

int test_rol_acc(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0x38;   /* SEC */
    bus.ram[0x0601] = 0xA9;   /* LDA #$00 */
    bus.ram[0x0602] = 0x00;
    bus.ram[0x0603] = 0x2A;   /* ROL A */
    cpu6502_step(&cpu);       /* SEC */
    cpu6502_step(&cpu);       /* LDA */
    cpu6502_step(&cpu);       /* ROL A */
    ASSERT(cpu.a == 0x01,       "test_rol_acc: A=%02X expected 01\n", cpu.a);
    ASSERT(!cpu_get_flag(&cpu, CPU_FLAG_C), "test_rol_acc: C set\n");
    return 0;
}

int test_ror_acc(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0x38;   /* SEC */
    bus.ram[0x0601] = 0xA9;   /* LDA #$00 */
    bus.ram[0x0602] = 0x00;
    bus.ram[0x0603] = 0x6A;   /* ROR A */
    cpu6502_step(&cpu);       /* SEC */
    cpu6502_step(&cpu);       /* LDA */
    cpu6502_step(&cpu);       /* ROR A */
    ASSERT(cpu.a == 0x80,       "test_ror_acc: A=%02X expected 80\n", cpu.a);
    ASSERT(!cpu_get_flag(&cpu, CPU_FLAG_C), "test_ror_acc: C set\n");
    ASSERT(cpu_get_flag(&cpu, CPU_FLAG_N),  "test_ror_acc: N not set\n");
    return 0;
}

/* ================================================================== */
/*  Increment / Decrement                                             */
/* ================================================================== */

int test_inx_basic(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA2;   /* LDX #$41 */
    bus.ram[0x0601] = 0x41;
    bus.ram[0x0602] = 0xE8;   /* INX */
    cpu6502_step(&cpu);       /* LDX */
    cpu6502_step(&cpu);       /* INX */
    ASSERT(cpu.x == 0x42, "test_inx_basic: X=%02X expected 42\n", cpu.x);
    return 0;
}

int test_inx_wrap(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA2;   /* LDX #$FF */
    bus.ram[0x0601] = 0xFF;
    bus.ram[0x0602] = 0xE8;   /* INX */
    cpu6502_step(&cpu);       /* LDX */
    cpu6502_step(&cpu);       /* INX */
    ASSERT(cpu.x == 0x00,       "test_inx_wrap: X=%02X expected 00\n", cpu.x);
    ASSERT(cpu_get_flag(&cpu, CPU_FLAG_Z),  "test_inx_wrap: Z not set\n");
    return 0;
}

int test_dex_basic(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA2;   /* LDX #$43 */
    bus.ram[0x0601] = 0x43;
    bus.ram[0x0602] = 0xCA;   /* DEX */
    cpu6502_step(&cpu);       /* LDX */
    cpu6502_step(&cpu);       /* DEX */
    ASSERT(cpu.x == 0x42, "test_dex_basic: X=%02X expected 42\n", cpu.x);
    return 0;
}

int test_iny_basic(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA0;   /* LDY #$41 */
    bus.ram[0x0601] = 0x41;
    bus.ram[0x0602] = 0xC8;   /* INY */
    cpu6502_step(&cpu);       /* LDY */
    cpu6502_step(&cpu);       /* INY */
    ASSERT(cpu.y == 0x42, "test_iny_basic: Y=%02X expected 42\n", cpu.y);
    return 0;
}

int test_dey_basic(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA0;   /* LDY #$43 */
    bus.ram[0x0601] = 0x43;
    bus.ram[0x0602] = 0x88;   /* DEY */
    cpu6502_step(&cpu);       /* LDY */
    cpu6502_step(&cpu);       /* DEY */
    ASSERT(cpu.y == 0x42, "test_dey_basic: Y=%02X expected 42\n", cpu.y);
    return 0;
}

int test_inc_zpg(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0010] = 0x41;   /* value at $10 */
    bus.ram[0x0600] = 0xE6;   /* INC $10 */
    bus.ram[0x0601] = 0x10;
    cpu6502_step(&cpu);
    ASSERT(bus.ram[0x0010] == 0x42,
           "test_inc_zpg: mem[$10]=%02X expected 42\n", bus.ram[0x0010]);
    return 0;
}

int test_dec_zpg(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0010] = 0x43;   /* value at $10 */
    bus.ram[0x0600] = 0xC6;   /* DEC $10 */
    bus.ram[0x0601] = 0x10;
    cpu6502_step(&cpu);
    ASSERT(bus.ram[0x0010] == 0x42,
           "test_dec_zpg: mem[$10]=%02X expected 42\n", bus.ram[0x0010]);
    return 0;
}

/* ================================================================== */
/*  Branches                                                          */
/* ================================================================== */

int test_bne_taken(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    /* LDX #$01 sets Z=0, so BNE should be taken */
    bus.ram[0x0600] = 0xA2;   /* LDX #$01 */
    bus.ram[0x0601] = 0x01;
    bus.ram[0x0602] = 0xD0;   /* BNE +2 (skip 2 bytes forward) */
    bus.ram[0x0603] = 0x02;   /* relative offset: +2 from $0604 -> $0606 */
    bus.ram[0x0604] = 0xEA;   /* NOP (should be skipped) */
    bus.ram[0x0605] = 0xEA;   /* NOP (should be skipped) */
    bus.ram[0x0606] = 0xEA;   /* NOP (landing target) */
    cpu6502_step(&cpu);       /* LDX */
    cpu6502_step(&cpu);       /* BNE */
    ASSERT(cpu.pc == 0x0606,
           "test_bne_taken: PC=%04X expected 0606\n", cpu.pc);
    return 0;
}

int test_bne_not_taken(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    /* LDA #$00 sets Z=1, so BNE should NOT be taken */
    bus.ram[0x0600] = 0xA9;   /* LDA #$00 */
    bus.ram[0x0601] = 0x00;
    bus.ram[0x0602] = 0xD0;   /* BNE +2 */
    bus.ram[0x0603] = 0x02;
    cpu6502_step(&cpu);       /* LDA */
    cpu6502_step(&cpu);       /* BNE (not taken) */
    /* PC should be at $0604 (past the BNE instruction) */
    ASSERT(cpu.pc == 0x0604,
           "test_bne_not_taken: PC=%04X expected 0604\n", cpu.pc);
    return 0;
}

int test_beq_taken(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA9;   /* LDA #$00 (sets Z=1) */
    bus.ram[0x0601] = 0x00;
    bus.ram[0x0602] = 0xF0;   /* BEQ +2 -> $0606 */
    bus.ram[0x0603] = 0x02;
    cpu6502_step(&cpu);       /* LDA */
    cpu6502_step(&cpu);       /* BEQ */
    ASSERT(cpu.pc == 0x0606,
           "test_beq_taken: PC=%04X expected 0606\n", cpu.pc);
    return 0;
}

int test_bcc_taken(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0x18;   /* CLC */
    bus.ram[0x0601] = 0x90;   /* BCC +2 -> $0605 */
    bus.ram[0x0602] = 0x02;
    cpu6502_step(&cpu);       /* CLC */
    cpu6502_step(&cpu);       /* BCC */
    ASSERT(cpu.pc == 0x0605,
           "test_bcc_taken: PC=%04X expected 0605\n", cpu.pc);
    return 0;
}

int test_bcs_taken(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0x38;   /* SEC */
    bus.ram[0x0601] = 0xB0;   /* BCS +2 -> $0605 */
    bus.ram[0x0602] = 0x02;
    cpu6502_step(&cpu);       /* SEC */
    cpu6502_step(&cpu);       /* BCS */
    ASSERT(cpu.pc == 0x0605,
           "test_bcs_taken: PC=%04X expected 0605\n", cpu.pc);
    return 0;
}

/* ================================================================== */
/*  Jumps                                                             */
/* ================================================================== */

int test_jmp_abs(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0x4C;   /* JMP $0700 */
    bus.ram[0x0601] = 0x00;
    bus.ram[0x0602] = 0x07;
    cpu6502_step(&cpu);
    ASSERT(cpu.pc == 0x0700,
           "test_jmp_abs: PC=%04X expected 0700\n", cpu.pc);
    return 0;
}

int test_jmp_ind_page_bug(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    /* JMP ($10FF) -- the 6502 page-boundary bug:
     * low byte comes from $10FF, high byte from $1000 (NOT $1100) */
    bus.ram[0x10FF] = 0x80;   /* low byte of target */
    bus.ram[0x1000] = 0x06;   /* high byte of target (wraps to page start) */
    bus.ram[0x1100] = 0xFF;   /* this should NOT be read (the bug) */
    bus.ram[0x0600] = 0x6C;   /* JMP ($10FF) */
    bus.ram[0x0601] = 0xFF;
    bus.ram[0x0602] = 0x10;
    cpu6502_step(&cpu);
    ASSERT(cpu.pc == 0x0680,
           "test_jmp_ind_page_bug: PC=%04X expected 0680\n", cpu.pc);
    return 0;
}

int test_jsr_rts(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    /* JSR $0700 at $0600, RTS at $0700
     * After RTS, PC should be $0603 (byte after the 3-byte JSR) */
    bus.ram[0x0600] = 0x20;   /* JSR $0700 */
    bus.ram[0x0601] = 0x00;
    bus.ram[0x0602] = 0x07;
    bus.ram[0x0700] = 0x60;   /* RTS */
    cpu6502_step(&cpu);       /* JSR */
    ASSERT(cpu.pc == 0x0700,
           "test_jsr_rts: after JSR PC=%04X expected 0700\n", cpu.pc);
    cpu6502_step(&cpu);       /* RTS */
    ASSERT(cpu.pc == 0x0603,
           "test_jsr_rts: after RTS PC=%04X expected 0603\n", cpu.pc);
    return 0;
}

/* ================================================================== */
/*  Stack                                                             */
/* ================================================================== */

int test_pha_pla(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xA9;   /* LDA #$42 */
    bus.ram[0x0601] = 0x42;
    bus.ram[0x0602] = 0x48;   /* PHA */
    bus.ram[0x0603] = 0xA9;   /* LDA #$00 */
    bus.ram[0x0604] = 0x00;
    bus.ram[0x0605] = 0x68;   /* PLA */
    cpu6502_step(&cpu);       /* LDA #$42 */
    cpu6502_step(&cpu);       /* PHA */
    cpu6502_step(&cpu);       /* LDA #$00 */
    ASSERT(cpu.a == 0x00, "test_pha_pla: A after LDA #00 = %02X\n", cpu.a);
    cpu6502_step(&cpu);       /* PLA */
    ASSERT(cpu.a == 0x42, "test_pha_pla: A=%02X expected 42\n", cpu.a);
    return 0;
}

int test_php_plp(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0x38;   /* SEC */
    bus.ram[0x0601] = 0xF8;   /* SED */
    bus.ram[0x0602] = 0x08;   /* PHP */
    bus.ram[0x0603] = 0x18;   /* CLC */
    bus.ram[0x0604] = 0xD8;   /* CLD */
    bus.ram[0x0605] = 0x28;   /* PLP */
    cpu6502_step(&cpu);       /* SEC */
    cpu6502_step(&cpu);       /* SED */
    cpu6502_step(&cpu);       /* PHP */
    cpu6502_step(&cpu);       /* CLC */
    cpu6502_step(&cpu);       /* CLD */
    /* Verify C and D are clear before PLP */
    ASSERT(!cpu_get_flag(&cpu, CPU_FLAG_C), "test_php_plp: C set before PLP\n");
    ASSERT(!cpu_get_flag(&cpu, CPU_FLAG_D), "test_php_plp: D set before PLP\n");
    cpu6502_step(&cpu);       /* PLP */
    ASSERT(cpu_get_flag(&cpu, CPU_FLAG_C),  "test_php_plp: C not restored\n");
    ASSERT(cpu_get_flag(&cpu, CPU_FLAG_D),  "test_php_plp: D not restored\n");
    return 0;
}

/* ================================================================== */
/*  Flags                                                             */
/* ================================================================== */

int test_sec_clc(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0x38;   /* SEC */
    bus.ram[0x0601] = 0x18;   /* CLC */
    cpu6502_step(&cpu);       /* SEC */
    ASSERT(cpu_get_flag(&cpu, CPU_FLAG_C),  "test_sec_clc: C not set after SEC\n");
    cpu6502_step(&cpu);       /* CLC */
    ASSERT(!cpu_get_flag(&cpu, CPU_FLAG_C), "test_sec_clc: C set after CLC\n");
    return 0;
}

int test_sei_cli(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    /* Note: I is already set after reset, so CLI first then SEI */
    bus.ram[0x0600] = 0x58;   /* CLI */
    bus.ram[0x0601] = 0x78;   /* SEI */
    cpu6502_step(&cpu);       /* CLI */
    ASSERT(!cpu_get_flag(&cpu, CPU_FLAG_I), "test_sei_cli: I set after CLI\n");
    cpu6502_step(&cpu);       /* SEI */
    ASSERT(cpu_get_flag(&cpu, CPU_FLAG_I),  "test_sei_cli: I not set after SEI\n");
    return 0;
}

int test_clv(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    /* Create overflow: $7F + $01 = $80 (signed: 127 + 1 = -128 -> V=1) */
    bus.ram[0x0600] = 0xA9;   /* LDA #$7F */
    bus.ram[0x0601] = 0x7F;
    bus.ram[0x0602] = 0x18;   /* CLC */
    bus.ram[0x0603] = 0x69;   /* ADC #$01 */
    bus.ram[0x0604] = 0x01;
    bus.ram[0x0605] = 0xB8;   /* CLV */
    cpu6502_step(&cpu);       /* LDA */
    cpu6502_step(&cpu);       /* CLC */
    cpu6502_step(&cpu);       /* ADC */
    ASSERT(cpu_get_flag(&cpu, CPU_FLAG_V),  "test_clv: V not set after overflow\n");
    cpu6502_step(&cpu);       /* CLV */
    ASSERT(!cpu_get_flag(&cpu, CPU_FLAG_V), "test_clv: V set after CLV\n");
    return 0;
}

/* ================================================================== */
/*  Interrupts                                                        */
/* ================================================================== */

int test_brk(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    /* Set up IRQ/BRK vector at $FFFE/$FFFF -> $0700 */
    bus.ram[0xFFFE] = 0x00;
    bus.ram[0xFFFF] = 0x07;
    /* Must clear I flag so we can observe BRK behavior properly.
     * BRK works regardless of I flag, but we want to verify the pushed status. */
    bus.ram[0x0600] = 0x58;   /* CLI (clear I so pushed status has I=0) */
    bus.ram[0x0601] = 0x00;   /* BRK */
    bus.ram[0x0602] = 0xEA;   /* padding byte (BRK skips this) */
    cpu6502_step(&cpu);       /* CLI */
    cpu6502_step(&cpu);       /* BRK */

    ASSERT(cpu.pc == 0x0700,
           "test_brk: PC=%04X expected 0700\n", cpu.pc);
    ASSERT(cpu_get_flag(&cpu, CPU_FLAG_I),
           "test_brk: I not set after BRK\n");

    /* Check pushed status byte on stack: B flag (bit 4) should be 1.
     * SP was $FD after reset, CLI is implied (no stack change).
     * BRK pushes: PChi, PClo, status -> SP goes from $FD to $FA.
     * Status byte is at $01FA+1 = $01FB (since SP points to next free). */
    uint8_t pushed_status = bus.ram[0x01FB];
    ASSERT(pushed_status & CPU_FLAG_B,
           "test_brk: pushed status B=0, expected B=1 (status=%02X)\n",
           pushed_status);
    return 0;
}

int test_irq_masked(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    /* Set up IRQ vector */
    bus.ram[0xFFFE] = 0x00;
    bus.ram[0xFFFF] = 0x07;
    /* After reset, I flag is already set. SEI to be explicit. */
    bus.ram[0x0600] = 0x78;   /* SEI */
    bus.ram[0x0601] = 0xEA;   /* NOP */
    cpu6502_step(&cpu);       /* SEI */

    uint16_t pc_before = cpu.pc;
    cpu6502_irq(&cpu);        /* trigger IRQ -- should be masked */

    ASSERT(cpu.pc == pc_before,
           "test_irq_masked: PC=%04X expected %04X (IRQ should be masked)\n",
           cpu.pc, pc_before);
    return 0;
}

int test_nmi(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    /* Set up NMI vector at $FFFA/$FFFB -> $0800 */
    bus.ram[0xFFFA] = 0x00;
    bus.ram[0xFFFB] = 0x08;
    bus.ram[0x0600] = 0xEA;   /* NOP (just to have something at reset) */
    cpu6502_step(&cpu);       /* NOP */
    cpu6502_nmi(&cpu);        /* trigger NMI */
    ASSERT(cpu.pc == 0x0800,
           "test_nmi: PC=%04X expected 0800\n", cpu.pc);
    return 0;
}

/* ================================================================== */
/*  Addressing Modes                                                  */
/* ================================================================== */

int test_zpx_wraps(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    /* LDA $FF,X with X=1 should read from ($FF+1) & 0xFF = $00 */
    bus.ram[0x0000] = 0x42;   /* value at zero-page $00 */
    bus.ram[0x0100] = 0xBB;   /* value at $0100 (should NOT be read) */
    bus.ram[0x0600] = 0xA2;   /* LDX #$01 */
    bus.ram[0x0601] = 0x01;
    bus.ram[0x0602] = 0xB5;   /* LDA $FF,X (zpg,X opcode = 0xB5) */
    bus.ram[0x0603] = 0xFF;
    cpu6502_step(&cpu);       /* LDX */
    cpu6502_step(&cpu);       /* LDA zpg,X */
    ASSERT(cpu.a == 0x42,
           "test_zpx_wraps: A=%02X expected 42 (wrapped to $00)\n", cpu.a);
    return 0;
}

int test_indexed_indirect(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    /* LDA ($20,X) with X=4:
     * Pointer address = ($20 + 4) & 0xFF = $24
     * $24/$25 contain the target address $1234
     * Value at $1234 = $42 */
    bus.ram[0x0024] = 0x34;   /* low byte of target addr */
    bus.ram[0x0025] = 0x12;   /* high byte of target addr */
    bus.ram[0x1234] = 0x42;   /* the value to load */
    bus.ram[0x0600] = 0xA2;   /* LDX #$04 */
    bus.ram[0x0601] = 0x04;
    bus.ram[0x0602] = 0xA1;   /* LDA ($20,X)  opcode = 0xA1 */
    bus.ram[0x0603] = 0x20;
    cpu6502_step(&cpu);       /* LDX */
    cpu6502_step(&cpu);       /* LDA (zp,X) */
    ASSERT(cpu.a == 0x42,
           "test_indexed_indirect: A=%02X expected 42\n", cpu.a);
    return 0;
}

int test_indirect_indexed(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    /* LDA ($20),Y with Y=4:
     * $20/$21 contain base address $1230
     * Effective address = $1230 + 4 = $1234
     * Value at $1234 = $42 */
    bus.ram[0x0020] = 0x30;   /* low byte of base addr */
    bus.ram[0x0021] = 0x12;   /* high byte of base addr */
    bus.ram[0x1234] = 0x42;   /* the value to load */
    bus.ram[0x0600] = 0xA0;   /* LDY #$04 */
    bus.ram[0x0601] = 0x04;
    bus.ram[0x0602] = 0xB1;   /* LDA ($20),Y  opcode = 0xB1 */
    bus.ram[0x0603] = 0x20;
    cpu6502_step(&cpu);       /* LDY */
    cpu6502_step(&cpu);       /* LDA (zp),Y */
    ASSERT(cpu.a == 0x42,
           "test_indirect_indexed: A=%02X expected 42\n", cpu.a);
    return 0;
}

/* ================================================================== */
/*  Cycle Counting                                                    */
/* ================================================================== */

int test_nop_cycles(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xEA;   /* NOP */
    uint64_t before = cpu.cycles;
    cpu6502_step(&cpu);
    uint64_t elapsed = cpu.cycles - before;
    ASSERT(elapsed == 2,
           "test_nop_cycles: cycles=%llu expected 2\n",
           (unsigned long long)elapsed);
    return 0;
}

int test_lda_abs_cycles(void)
{
    bus_flat_t bus; cpu6502_t cpu;
    setup(&bus, &cpu);
    bus.ram[0x0600] = 0xAD;   /* LDA $1234 */
    bus.ram[0x0601] = 0x34;
    bus.ram[0x0602] = 0x12;
    uint64_t before = cpu.cycles;
    cpu6502_step(&cpu);
    uint64_t elapsed = cpu.cycles - before;
    ASSERT(elapsed == 4,
           "test_lda_abs_cycles: cycles=%llu expected 4\n",
           (unsigned long long)elapsed);
    return 0;
}
