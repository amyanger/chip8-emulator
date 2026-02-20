#include <stdio.h>

/* ------------------------------------------------------------------ */
/*  Extern declarations for all test functions from test_opcodes.c    */
/* ------------------------------------------------------------------ */

/* Load / Store */
extern int test_lda_imm_basic(void);
extern int test_lda_imm_zero(void);
extern int test_lda_imm_negative(void);
extern int test_lda_zpg(void);
extern int test_lda_abs(void);
extern int test_lda_abx_page_cross(void);
extern int test_sta_zpg(void);
extern int test_sta_abs(void);
extern int test_ldx_imm(void);
extern int test_ldy_imm(void);

/* Arithmetic */
extern int test_adc_no_carry(void);
extern int test_adc_with_carry_in(void);
extern int test_adc_carry_out(void);
extern int test_adc_overflow_pos(void);
extern int test_sbc_basic(void);
extern int test_sbc_borrow(void);

/* Compare */
extern int test_cmp_equal(void);
extern int test_cmp_greater(void);
extern int test_cmp_less(void);

/* Logical */
extern int test_and_basic(void);
extern int test_ora_basic(void);
extern int test_eor_basic(void);

/* Shifts */
extern int test_asl_acc(void);
extern int test_lsr_acc(void);
extern int test_rol_acc(void);
extern int test_ror_acc(void);

/* Inc / Dec */
extern int test_inx_basic(void);
extern int test_inx_wrap(void);
extern int test_dex_basic(void);
extern int test_iny_basic(void);
extern int test_dey_basic(void);
extern int test_inc_zpg(void);
extern int test_dec_zpg(void);

/* Branches */
extern int test_bne_taken(void);
extern int test_bne_not_taken(void);
extern int test_beq_taken(void);
extern int test_bcc_taken(void);
extern int test_bcs_taken(void);

/* Jumps */
extern int test_jmp_abs(void);
extern int test_jmp_ind_page_bug(void);
extern int test_jsr_rts(void);

/* Stack */
extern int test_pha_pla(void);
extern int test_php_plp(void);

/* Flags */
extern int test_sec_clc(void);
extern int test_sei_cli(void);
extern int test_clv(void);

/* Interrupts */
extern int test_brk(void);
extern int test_irq_masked(void);
extern int test_nmi(void);

/* Addressing modes */
extern int test_zpx_wraps(void);
extern int test_indexed_indirect(void);
extern int test_indirect_indexed(void);

/* Cycle counting */
extern int test_nop_cycles(void);
extern int test_lda_abs_cycles(void);

/* ------------------------------------------------------------------ */
/*  Test runner                                                       */
/* ------------------------------------------------------------------ */

typedef int (*test_fn)(void);

typedef struct {
    const char *name;
    test_fn fn;
} test_entry;

int main(void)
{
    test_entry tests[] = {
        /* Load / Store */
        {"lda_imm_basic",       test_lda_imm_basic},
        {"lda_imm_zero",        test_lda_imm_zero},
        {"lda_imm_negative",    test_lda_imm_negative},
        {"lda_zpg",             test_lda_zpg},
        {"lda_abs",             test_lda_abs},
        {"lda_abx_page_cross",  test_lda_abx_page_cross},
        {"sta_zpg",             test_sta_zpg},
        {"sta_abs",             test_sta_abs},
        {"ldx_imm",             test_ldx_imm},
        {"ldy_imm",             test_ldy_imm},

        /* Arithmetic */
        {"adc_no_carry",        test_adc_no_carry},
        {"adc_with_carry_in",   test_adc_with_carry_in},
        {"adc_carry_out",       test_adc_carry_out},
        {"adc_overflow_pos",    test_adc_overflow_pos},
        {"sbc_basic",           test_sbc_basic},
        {"sbc_borrow",          test_sbc_borrow},

        /* Compare */
        {"cmp_equal",           test_cmp_equal},
        {"cmp_greater",         test_cmp_greater},
        {"cmp_less",            test_cmp_less},

        /* Logical */
        {"and_basic",           test_and_basic},
        {"ora_basic",           test_ora_basic},
        {"eor_basic",           test_eor_basic},

        /* Shifts */
        {"asl_acc",             test_asl_acc},
        {"lsr_acc",             test_lsr_acc},
        {"rol_acc",             test_rol_acc},
        {"ror_acc",             test_ror_acc},

        /* Inc / Dec */
        {"inx_basic",           test_inx_basic},
        {"inx_wrap",            test_inx_wrap},
        {"dex_basic",           test_dex_basic},
        {"iny_basic",           test_iny_basic},
        {"dey_basic",           test_dey_basic},
        {"inc_zpg",             test_inc_zpg},
        {"dec_zpg",             test_dec_zpg},

        /* Branches */
        {"bne_taken",           test_bne_taken},
        {"bne_not_taken",       test_bne_not_taken},
        {"beq_taken",           test_beq_taken},
        {"bcc_taken",           test_bcc_taken},
        {"bcs_taken",           test_bcs_taken},

        /* Jumps */
        {"jmp_abs",             test_jmp_abs},
        {"jmp_ind_page_bug",    test_jmp_ind_page_bug},
        {"jsr_rts",             test_jsr_rts},

        /* Stack */
        {"pha_pla",             test_pha_pla},
        {"php_plp",             test_php_plp},

        /* Flags */
        {"sec_clc",             test_sec_clc},
        {"sei_cli",             test_sei_cli},
        {"clv",                 test_clv},

        /* Interrupts */
        {"brk",                 test_brk},
        {"irq_masked",          test_irq_masked},
        {"nmi",                 test_nmi},

        /* Addressing modes */
        {"zpx_wraps",           test_zpx_wraps},
        {"indexed_indirect",    test_indexed_indirect},
        {"indirect_indexed",    test_indirect_indexed},

        /* Cycle counting */
        {"nop_cycles",          test_nop_cycles},
        {"lda_abs_cycles",      test_lda_abs_cycles},
    };

    int total = (int)(sizeof(tests) / sizeof(tests[0]));
    int passed = 0;
    int failed = 0;

    for (int i = 0; i < total; i++) {
        if (tests[i].fn() == 0) {
            passed++;
        } else {
            fprintf(stderr, "  FAILED: %s\n", tests[i].name);
            failed++;
        }
    }

    printf("\n%d/%d tests passed", passed, total);
    if (failed > 0)
        printf(", %d FAILED", failed);
    printf("\n");

    return failed > 0 ? 1 : 0;
}
