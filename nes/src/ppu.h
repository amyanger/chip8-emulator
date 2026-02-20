#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include <stdbool.h>

#define NES_WIDTH  256
#define NES_HEIGHT 240

typedef struct nes_t nes_t;

typedef struct {
    /* VRAM */
    uint8_t nametable[2048];
    uint8_t palette[32];
    uint8_t oam[256];

    /* Registers */
    uint8_t ctrl;       /* $2000 PPUCTRL */
    uint8_t mask;       /* $2001 PPUMASK */
    uint8_t status;     /* $2002 PPUSTATUS */
    uint8_t oam_addr;   /* $2003 OAMADDR */

    /* Internal registers (Loopy model) */
    uint16_t v;         /* current VRAM address (15 bits) */
    uint16_t t;         /* temporary VRAM address (15 bits) */
    uint8_t  fine_x;    /* fine X scroll (3 bits) */
    bool     w;         /* write toggle */

    /* Data read buffer for $2007 */
    uint8_t data_buf;

    /* Scanline state */
    int      scanline;  /* -1 (pre-render) to 260 */
    int      cycle;     /* 0-340 */
    uint64_t frame;

    /* NMI */
    bool nmi_occurred;
    bool nmi_output;

    /* Output framebuffer: 256x240 pixels as ARGB8888 */
    uint32_t framebuffer[NES_WIDTH * NES_HEIGHT];

    /* Back-pointer to NES for CHR access */
    nes_t *nes;
} ppu_t;

void    ppu_init(ppu_t *ppu, nes_t *nes);
void    ppu_reset(ppu_t *ppu);
bool    ppu_step(ppu_t *ppu);  /* returns true if NMI should fire */

/* CPU-facing register interface (addr is 0-7) */
uint8_t ppu_cpu_read(ppu_t *ppu, uint16_t addr);
void    ppu_cpu_write(ppu_t *ppu, uint16_t addr, uint8_t val);

/* PPU internal bus */
uint8_t ppu_bus_read(ppu_t *ppu, uint16_t addr);
void    ppu_bus_write(ppu_t *ppu, uint16_t addr, uint8_t val);

#endif
