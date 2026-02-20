#ifndef NES_H
#define NES_H

#include <stdint.h>
#include <stdbool.h>
#include "cpu6502.h"
#include "ppu.h"
#include "cartridge.h"

struct nes_t {
    cpu6502_t   cpu;
    ppu_t       ppu;
    cartridge_t cart;

    /* 2KB internal RAM */
    uint8_t ram[2048];

    /* Controllers */
    uint8_t controller[2];        /* current button state */
    uint8_t controller_shift[2];  /* shift register for serial reads */
    bool    controller_strobe;

    /* OAM DMA */
    bool     dma_pending;
    uint8_t  dma_page;
    uint16_t dma_addr;
    bool     dma_dummy;

    /* Timing */
    uint64_t system_cycles;
};

bool nes_init(nes_t *nes, const char *rom_path);
void nes_free(nes_t *nes);
void nes_step_frame(nes_t *nes);
void nes_set_controller(nes_t *nes, int port, uint8_t buttons);

/* NES CPU bus (match bus_read_fn / bus_write_fn signatures) */
uint8_t nes_bus_read(void *ctx, uint16_t addr);
void    nes_bus_write(void *ctx, uint16_t addr, uint8_t val);

/* Controller button bits */
#define BTN_A      0x01
#define BTN_B      0x02
#define BTN_SELECT 0x04
#define BTN_START  0x08
#define BTN_UP     0x10
#define BTN_DOWN   0x20
#define BTN_LEFT   0x40
#define BTN_RIGHT  0x80

#endif
