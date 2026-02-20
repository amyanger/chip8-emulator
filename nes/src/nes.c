#include <stdio.h>
#include <string.h>

#include "nes.h"

/* ---------------------------------------------------------------------------
 * NES CPU bus read ($0000-$FFFF)
 * Routes CPU addresses to the appropriate subsystem.
 * ------------------------------------------------------------------------- */
uint8_t nes_bus_read(void *ctx, uint16_t addr)
{
    nes_t *nes = (nes_t *)ctx;

    if (addr < 0x2000) {
        /* $0000-$1FFF: 2KB internal RAM, mirrored every 0x0800 */
        return nes->ram[addr & 0x07FF];
    }

    if (addr < 0x4000) {
        /* $2000-$3FFF: PPU registers, mirrored every 8 bytes */
        return ppu_cpu_read(&nes->ppu, addr & 0x0007);
    }

    if (addr == 0x4016) {
        /* Controller 1: serial read — return bit 0, then shift right */
        uint8_t bit = nes->controller_shift[0] & 0x01;
        nes->controller_shift[0] >>= 1;
        return bit;
    }

    if (addr == 0x4017) {
        /* Controller 2: serial read — return bit 0, then shift right */
        uint8_t bit = nes->controller_shift[1] & 0x01;
        nes->controller_shift[1] >>= 1;
        return bit;
    }

    if (addr < 0x4020) {
        /* $4000-$4015, $4018-$401F: APU/IO registers — stub */
        return 0;
    }

    /* $4020-$FFFF: cartridge space */
    return cartridge_cpu_read(&nes->cart, addr);
}

/* ---------------------------------------------------------------------------
 * NES CPU bus write ($0000-$FFFF)
 * Routes CPU writes to the appropriate subsystem.
 * ------------------------------------------------------------------------- */
void nes_bus_write(void *ctx, uint16_t addr, uint8_t val)
{
    nes_t *nes = (nes_t *)ctx;

    if (addr < 0x2000) {
        /* $0000-$1FFF: 2KB internal RAM, mirrored */
        nes->ram[addr & 0x07FF] = val;
        return;
    }

    if (addr < 0x4000) {
        /* $2000-$3FFF: PPU registers, mirrored every 8 bytes */
        ppu_cpu_write(&nes->ppu, addr & 0x0007, val);
        return;
    }

    if (addr == 0x4014) {
        /* OAM DMA: writing $XX here copies 256 bytes from page $XX00 */
        nes->dma_pending = true;
        nes->dma_page = val;
        return;
    }

    if (addr == 0x4016) {
        /* Controller strobe */
        if (val & 0x01) {
            nes->controller_strobe = true;
        } else if (nes->controller_strobe) {
            /* Falling edge: latch current button state into shift registers */
            nes->controller_shift[0] = nes->controller[0];
            nes->controller_shift[1] = nes->controller[1];
            nes->controller_strobe = false;
        }
        return;
    }

    if (addr < 0x4020) {
        /* $4000-$4013, $4015, $4017-$401F: APU/IO registers — stub, no-op */
        return;
    }

    /* $4020-$FFFF: cartridge space */
    cartridge_cpu_write(&nes->cart, addr, val);
}

/* ---------------------------------------------------------------------------
 * Initialization and teardown
 * ------------------------------------------------------------------------- */
bool nes_init(nes_t *nes, const char *rom_path)
{
    if (!nes || !rom_path) {
        fprintf(stderr, "nes_init: NULL argument\n");
        return false;
    }

    memset(nes, 0, sizeof(*nes));

    if (!cartridge_load(&nes->cart, rom_path)) {
        fprintf(stderr, "nes_init: failed to load ROM '%s'\n", rom_path);
        return false;
    }

    ppu_init(&nes->ppu, nes);
    cpu6502_init(&nes->cpu, nes_bus_read, nes_bus_write, nes);
    cpu6502_reset(&nes->cpu);

    return true;
}

void nes_free(nes_t *nes)
{
    if (!nes)
        return;
    cartridge_free(&nes->cart);
}

/* ---------------------------------------------------------------------------
 * Run one complete frame (~29780.5 CPU cycles, 89341.5 PPU cycles)
 *
 * Keeps stepping the CPU and PPU (at 3:1 ratio) until the PPU's frame
 * counter advances. OAM DMA is handled inline when dma_pending is set.
 * ------------------------------------------------------------------------- */
void nes_step_frame(nes_t *nes)
{
    uint64_t start_frame = nes->ppu.frame;

    while (nes->ppu.frame == start_frame) {
        if (nes->dma_pending) {
            /* OAM DMA: copy 256 bytes from CPU page $XX00 into OAM */
            for (int i = 0; i < 256; i++) {
                uint16_t src = ((uint16_t)nes->dma_page << 8) | (uint16_t)i;
                nes->ppu.oam[i] = nes_bus_read(nes, src);
            }
            nes->dma_pending = false;

            /* DMA takes ~514 CPU cycles = ~1542 PPU cycles */
            for (int i = 0; i < 1542; i++) {
                if (ppu_step(&nes->ppu)) {
                    cpu6502_nmi(&nes->cpu);
                }
            }
            nes->cpu.cycles += 514;
        } else {
            uint64_t prev = nes->cpu.cycles;
            cpu6502_step(&nes->cpu);
            uint64_t elapsed = nes->cpu.cycles - prev;

            /* PPU runs at 3x the CPU clock */
            for (uint64_t i = 0; i < elapsed * 3; i++) {
                if (ppu_step(&nes->ppu)) {
                    cpu6502_nmi(&nes->cpu);
                }
            }
        }
    }
}

/* ---------------------------------------------------------------------------
 * Set controller button state
 * port: 0 or 1
 * buttons: bitmask using BTN_A, BTN_B, etc.
 * ------------------------------------------------------------------------- */
void nes_set_controller(nes_t *nes, int port, uint8_t buttons)
{
    if (!nes)
        return;
    if (port < 0 || port > 1)
        return;
    nes->controller[port] = buttons;
}
