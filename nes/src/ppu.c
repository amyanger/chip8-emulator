/* ppu.c -- NES Picture Processing Unit (2C02) scanline renderer
 *
 * Renders 262 scanlines per frame, 341 PPU cycles per scanline.
 * Uses a scanline-based approach: at cycle 0 of each visible scanline (0-239),
 * the entire scanline is rendered at once. Not cycle-accurate, but sufficient
 * for Mapper 0 games.
 */

#include <stdio.h>
#include <string.h>

#include "nes.h"

/* -----------------------------------------------------------------------
 * Standard 2C02 palette (64 entries, ARGB8888)
 * ----------------------------------------------------------------------- */
static const uint32_t nes_palette[64] = {
    0xFF666666, 0xFF002A88, 0xFF1412A7, 0xFF3B00A4,
    0xFF5C007E, 0xFF6E0040, 0xFF6C0600, 0xFF561D00,
    0xFF333500, 0xFF0B4800, 0xFF005200, 0xFF004F08,
    0xFF00404D, 0xFF000000, 0xFF000000, 0xFF000000,

    0xFFADADAD, 0xFF155FD9, 0xFF4240FF, 0xFF7527FE,
    0xFFA01ACC, 0xFFB71E7B, 0xFFB53120, 0xFF994E00,
    0xFF6B6D00, 0xFF388700, 0xFF0C9300, 0xFF008F32,
    0xFF007C8D, 0xFF000000, 0xFF000000, 0xFF000000,

    0xFFFFFFFF, 0xFF64B0FF, 0xFF9290FF, 0xFFC676FF,
    0xFFF36AFF, 0xFFFE6ECC, 0xFFFE8170, 0xFFEA9E22,
    0xFFBCBE00, 0xFF88D800, 0xFF5CE430, 0xFF45E082,
    0xFF48CDDE, 0xFF4F4F4F, 0xFF000000, 0xFF000000,

    0xFFFFFFFF, 0xFFC0DFFF, 0xFFD3D2FF, 0xFFE8C8FF,
    0xFFFBC2FF, 0xFFFEC4EA, 0xFFFECCC5, 0xFFF7D8A5,
    0xFFE4E594, 0xFFCFEF96, 0xFFBDF4AB, 0xFFB3F3CC,
    0xFFB5EBF2, 0xFFB8B8B8, 0xFF000000, 0xFF000000,
};

/* Forward declaration for the scanline renderer */
static void render_scanline(ppu_t *ppu);

/* -----------------------------------------------------------------------
 * PPU internal bus: read
 * Maps $0000-$1FFF to cartridge CHR, $2000-$3EFF to nametables,
 * and $3F00-$3FFF to palette RAM.
 * ----------------------------------------------------------------------- */
uint8_t ppu_bus_read(ppu_t *ppu, uint16_t addr)
{
    addr &= 0x3FFF;

    if (addr < 0x2000) {
        /* Pattern tables: routed to cartridge CHR ROM/RAM */
        return cartridge_chr_read(&ppu->nes->cart, addr);
    }

    if (addr < 0x3F00) {
        /* Nametables with mirroring */
        addr &= 0x2FFF;  /* Handle $3000-$3EFF mirror of $2000-$2EFF */
        uint16_t offset = addr - 0x2000;
        uint16_t table  = offset / 0x0400;
        uint16_t index  = offset % 0x0400;
        uint16_t nt_index;

        if (ppu->nes->cart.mirror == MIRROR_VERTICAL) {
            /* Tables 0,2 share physical NT 0; tables 1,3 share physical NT 1 */
            nt_index = (table & 1) * 0x0400 + index;
        } else {
            /* Horizontal: tables 0,1 share NT 0; tables 2,3 share NT 1 */
            nt_index = (table / 2) * 0x0400 + index;
        }
        return ppu->nametable[nt_index];
    }

    /* Palette RAM ($3F00-$3FFF) */
    uint16_t idx = addr & 0x1F;
    /* Mirror sprite palette background entries to BG palette */
    if (idx == 0x10) idx = 0x00;
    if (idx == 0x14) idx = 0x04;
    if (idx == 0x18) idx = 0x08;
    if (idx == 0x1C) idx = 0x0C;
    return ppu->palette[idx];
}

/* -----------------------------------------------------------------------
 * PPU internal bus: write
 * Same routing logic as read.
 * ----------------------------------------------------------------------- */
void ppu_bus_write(ppu_t *ppu, uint16_t addr, uint8_t val)
{
    addr &= 0x3FFF;

    if (addr < 0x2000) {
        /* Pattern tables: CHR writes only effective for CHR RAM */
        cartridge_chr_write(&ppu->nes->cart, addr, val);
        return;
    }

    if (addr < 0x3F00) {
        /* Nametables with mirroring */
        addr &= 0x2FFF;
        uint16_t offset = addr - 0x2000;
        uint16_t table  = offset / 0x0400;
        uint16_t index  = offset % 0x0400;
        uint16_t nt_index;

        if (ppu->nes->cart.mirror == MIRROR_VERTICAL) {
            nt_index = (table & 1) * 0x0400 + index;
        } else {
            nt_index = (table / 2) * 0x0400 + index;
        }
        ppu->nametable[nt_index] = val;
        return;
    }

    /* Palette RAM: only 6 bits are valid */
    uint16_t idx = addr & 0x1F;
    if (idx == 0x10) idx = 0x00;
    if (idx == 0x14) idx = 0x04;
    if (idx == 0x18) idx = 0x08;
    if (idx == 0x1C) idx = 0x0C;
    ppu->palette[idx] = val & 0x3F;
}

/* -----------------------------------------------------------------------
 * CPU-facing register write ($2000-$2007)
 * addr is the full CPU address; only the low 3 bits matter.
 * ----------------------------------------------------------------------- */
void ppu_cpu_write(ppu_t *ppu, uint16_t addr, uint8_t val)
{
    switch (addr & 0x07) {
    case 0: { /* $2000 PPUCTRL */
        ppu->ctrl = val;
        bool prev_nmi = ppu->nmi_output;
        ppu->nmi_output = (val & 0x80) != 0;
        /* Load nametable select bits into t */
        ppu->t = (ppu->t & 0xF3FF) | (((uint16_t)(val & 0x03)) << 10);
        /* NMI edge: if nmi_output went 0->1 while nmi_occurred is set */
        if (!prev_nmi && ppu->nmi_output && ppu->nmi_occurred) {
            /* Re-assert NMI -- the caller should check nmi_occurred */
            ppu->nmi_occurred = true;
        }
        break;
    }
    case 1: /* $2001 PPUMASK */
        ppu->mask = val;
        break;

    case 3: /* $2003 OAMADDR */
        ppu->oam_addr = val;
        break;

    case 4: /* $2004 OAMDATA */
        ppu->oam[ppu->oam_addr] = val;
        ppu->oam_addr++;
        break;

    case 5: /* $2005 PPUSCROLL */
        if (!ppu->w) {
            /* First write: coarse X + fine X */
            ppu->t = (ppu->t & 0xFFE0) | ((uint16_t)(val >> 3));
            ppu->fine_x = val & 0x07;
            ppu->w = true;
        } else {
            /* Second write: coarse Y + fine Y */
            ppu->t = (ppu->t & 0x8C1F)
                    | (((uint16_t)(val & 0x07)) << 12)
                    | (((uint16_t)(val & 0xF8)) << 2);
            ppu->w = false;
        }
        break;

    case 6: /* $2006 PPUADDR */
        if (!ppu->w) {
            /* First write: high byte (only low 6 bits used, bit 14 cleared) */
            ppu->t = (ppu->t & 0x00FF) | (((uint16_t)(val & 0x3F)) << 8);
            ppu->w = true;
        } else {
            /* Second write: low byte, then copy t into v */
            ppu->t = (ppu->t & 0xFF00) | (uint16_t)val;
            ppu->v = ppu->t;
            ppu->w = false;
        }
        break;

    case 7: /* $2007 PPUDATA */
        ppu_bus_write(ppu, ppu->v, val);
        ppu->v += (ppu->ctrl & 0x04) ? 32 : 1;
        break;

    default:
        /* Writes to $2002 are ignored */
        break;
    }
}

/* -----------------------------------------------------------------------
 * CPU-facing register read ($2000-$2007)
 * ----------------------------------------------------------------------- */
uint8_t ppu_cpu_read(ppu_t *ppu, uint16_t addr)
{
    switch (addr & 0x07) {
    case 2: { /* $2002 PPUSTATUS */
        uint8_t result = ppu->status & 0xE0;
        if (ppu->nmi_occurred)
            result |= 0x80;
        /* Reading status clears VBlank flag and the write toggle */
        ppu->nmi_occurred = false;
        ppu->w = false;
        return result;
    }
    case 4: /* $2004 OAMDATA */
        return ppu->oam[ppu->oam_addr];

    case 7: { /* $2007 PPUDATA */
        uint8_t data;
        if (ppu->v < 0x3F00) {
            /* Non-palette read: return buffered value, then fill buffer */
            data = ppu->data_buf;
            ppu->data_buf = ppu_bus_read(ppu, ppu->v);
        } else {
            /* Palette read: return palette value directly,
             * but fill buffer from the nametable "underneath" */
            data = ppu_bus_read(ppu, ppu->v);
            ppu->data_buf = ppu_bus_read(ppu, ppu->v - 0x1000);
        }
        ppu->v += (ppu->ctrl & 0x04) ? 32 : 1;
        return data;
    }
    default:
        break;
    }
    return 0;
}

/* -----------------------------------------------------------------------
 * render_scanline -- renders a full 256-pixel row (background + sprites)
 *
 * Called at cycle 0 of each visible scanline (0-239) when rendering is
 * enabled. Uses the PPU v register for background scrolling and evaluates
 * up to 8 sprites per scanline.
 * ----------------------------------------------------------------------- */
static void render_scanline(ppu_t *ppu)
{
    int y = ppu->scanline;

    /* Background pixel/palette buffers for compositing */
    uint8_t bg_pixel[256];
    uint8_t bg_palette[256];
    memset(bg_pixel, 0, sizeof(bg_pixel));
    memset(bg_palette, 0, sizeof(bg_palette));

    /* --- Background rendering --- */
    if (ppu->mask & 0x08) {
        uint16_t v = ppu->v;

        /* Render 33 tiles (one extra for fine-X scrolling overshoot) */
        for (int tile = 0; tile < 33; tile++) {
            /* Nametable byte */
            uint16_t nt_addr = 0x2000 | (v & 0x0FFF);
            uint8_t tile_id = ppu_bus_read(ppu, nt_addr);

            /* Attribute byte: determines the palette for this 16x16 area */
            uint16_t attr_addr = 0x23C0
                               | (v & 0x0C00)
                               | ((v >> 4) & 0x38)
                               | ((v >> 2) & 0x07);
            uint8_t attr_byte = ppu_bus_read(ppu, attr_addr);
            uint8_t shift = ((v >> 4) & 0x04) | (v & 0x02);
            uint8_t pal = (attr_byte >> shift) & 0x03;

            /* Pattern table address */
            uint16_t pattern_base = (ppu->ctrl & 0x10) ? 0x1000 : 0x0000;
            uint8_t fine_y = (v >> 12) & 0x07;
            uint8_t plane0 = ppu_bus_read(ppu, pattern_base + tile_id * 16 + fine_y);
            uint8_t plane1 = ppu_bus_read(ppu, pattern_base + tile_id * 16 + fine_y + 8);

            /* Decode 8 pixels from the tile row */
            for (int px = 0; px < 8; px++) {
                int screen_x = tile * 8 + px - ppu->fine_x;
                if (screen_x < 0 || screen_x >= 256) continue;

                int bit = 7 - px;
                uint8_t pixel = (uint8_t)(((plane1 >> bit) & 1) << 1
                              | ((plane0 >> bit) & 1));
                bg_pixel[screen_x]   = pixel;
                bg_palette[screen_x] = pal;
            }

            /* Increment coarse X in the local v copy */
            if ((v & 0x001F) == 31) {
                v &= ~0x001F;
                v ^= 0x0400;   /* Switch horizontal nametable */
            } else {
                v++;
            }
        }
    }

    /* --- Sprite evaluation and rendering --- */
    uint8_t spr_color[256];
    uint8_t spr_priority[256];   /* 0 = in front of BG, 1 = behind BG */
    uint8_t spr_opaque[256];
    uint8_t spr_zero[256];
    memset(spr_opaque, 0, sizeof(spr_opaque));
    memset(spr_zero, 0, sizeof(spr_zero));

    if (ppu->mask & 0x10) {
        int sprite_height = (ppu->ctrl & 0x20) ? 16 : 8;
        int count = 0;

        /* Find up to 8 sprites that overlap this scanline */
        int sprite_indices[8];
        for (int i = 0; i < 64 && count < 8; i++) {
            uint8_t sy = ppu->oam[i * 4];
            int row = y - ((int)sy + 1);
            if (row >= 0 && row < sprite_height) {
                sprite_indices[count++] = i;
            }
        }

        /* Render in reverse order so lower-index sprites overwrite higher
         * (lower OAM index = higher priority, painted last) */
        for (int s = count - 1; s >= 0; s--) {
            int i        = sprite_indices[s];
            uint8_t sy   = ppu->oam[i * 4 + 0];
            uint8_t tile = ppu->oam[i * 4 + 1];
            uint8_t attr = ppu->oam[i * 4 + 2];
            uint8_t sx   = ppu->oam[i * 4 + 3];

            int row = y - ((int)sy + 1);

            /* Vertical flip */
            if (attr & 0x80) row = sprite_height - 1 - row;

            /* Compute pattern address */
            uint16_t pattern_addr;
            if (sprite_height == 8) {
                uint16_t table = (ppu->ctrl & 0x08) ? 0x1000 : 0x0000;
                pattern_addr = table + tile * 16 + row;
            } else {
                /* 8x16 sprites: bank selected by bit 0 of tile index */
                uint16_t table    = (tile & 1) ? 0x1000 : 0x0000;
                uint8_t  tile_num = tile & 0xFE;
                if (row >= 8) { tile_num++; row -= 8; }
                pattern_addr = table + tile_num * 16 + row;
            }

            uint8_t plane0 = ppu_bus_read(ppu, pattern_addr);
            uint8_t plane1 = ppu_bus_read(ppu, pattern_addr + 8);

            for (int px = 0; px < 8; px++) {
                int bit = (attr & 0x40) ? px : (7 - px);  /* Horizontal flip */
                uint8_t pixel = (uint8_t)(((plane1 >> bit) & 1) << 1
                              | ((plane0 >> bit) & 1));
                if (pixel == 0) continue;  /* Transparent */

                int screen_x = sx + px;
                if (screen_x >= 256) continue;

                uint8_t spr_pal = attr & 0x03;
                uint16_t pal_addr = 0x10 + spr_pal * 4 + pixel;
                /* Mirror sprite background ($10) to universal BG ($00) */
                if (pal_addr == 0x10) pal_addr = 0x00;

                spr_color[screen_x]    = ppu->palette[pal_addr];
                spr_priority[screen_x] = (attr >> 5) & 1;
                spr_opaque[screen_x]   = 1;
                if (i == 0) spr_zero[screen_x] = 1;
            }
        }
    }

    /* --- Composite background and sprites into the framebuffer --- */
    for (int x = 0; x < 256; x++) {
        bool show_bg  = (bg_pixel[x] != 0)
                      && (ppu->mask & 0x08)
                      && (x >= 8 || (ppu->mask & 0x02));
        bool show_spr = spr_opaque[x]
                      && (ppu->mask & 0x10)
                      && (x >= 8 || (ppu->mask & 0x04));

        /* Sprite 0 hit detection */
        if (show_bg && show_spr && spr_zero[x] && x != 255) {
            ppu->status |= 0x40;
        }

        uint8_t color;
        if (!show_bg && !show_spr) {
            /* Neither BG nor sprite: use universal background color */
            color = ppu->palette[0];
        } else if (show_spr && !show_bg) {
            color = spr_color[x];
        } else if (!show_spr && show_bg) {
            color = ppu->palette[bg_palette[x] * 4 + bg_pixel[x]];
        } else {
            /* Both opaque: sprite priority decides */
            if (spr_priority[x] == 0) {
                color = spr_color[x];       /* Sprite in front */
            } else {
                color = ppu->palette[bg_palette[x] * 4 + bg_pixel[x]];
            }
        }

        ppu->framebuffer[y * NES_WIDTH + x] = nes_palette[color & 0x3F];
    }

    /* --- Post-scanline v register updates --- */

    /* Increment fine Y, with carry into coarse Y */
    if ((ppu->v & 0x7000) != 0x7000) {
        ppu->v += 0x1000;
    } else {
        ppu->v &= ~0x7000;
        int cy = (ppu->v & 0x03E0) >> 5;
        if (cy == 29) {
            cy = 0;
            ppu->v ^= 0x0800;   /* Switch vertical nametable */
        } else if (cy == 31) {
            cy = 0;              /* Wrap without toggling nametable */
        } else {
            cy++;
        }
        ppu->v = (ppu->v & ~0x03E0) | (cy << 5);
    }

    /* Copy horizontal bits from t into v (reset X scroll for next line) */
    ppu->v = (ppu->v & ~0x041F) | (ppu->t & 0x041F);
}

/* -----------------------------------------------------------------------
 * ppu_step -- advance the PPU by one cycle
 *
 * Returns true if an NMI should be sent to the CPU (VBlank start with
 * NMI output enabled).
 * ----------------------------------------------------------------------- */
bool ppu_step(ppu_t *ppu)
{
    bool nmi_triggered = false;
    bool rendering_enabled = (ppu->mask & 0x18) != 0;

    if (ppu->scanline == -1) {
        /* Pre-render scanline */
        if (ppu->cycle == 1) {
            /* Clear VBlank, sprite 0 hit, and sprite overflow flags */
            ppu->nmi_occurred = false;
            ppu->status &= ~0xE0;
        }
        if (rendering_enabled && ppu->cycle >= 280 && ppu->cycle <= 304) {
            /* Copy all vertical position bits from t to v */
            ppu->v = (ppu->v & ~0x7BE0) | (ppu->t & 0x7BE0);
        }
    } else if (ppu->scanline >= 0 && ppu->scanline < 240) {
        /* Visible scanlines */
        if (ppu->cycle == 0 && rendering_enabled) {
            render_scanline(ppu);
        }
    } else if (ppu->scanline == 241 && ppu->cycle == 1) {
        /* VBlank start */
        ppu->nmi_occurred = true;
        ppu->status |= 0x80;
        if (ppu->nmi_output) {
            nmi_triggered = true;
        }
    }

    /* Advance to the next cycle/scanline */
    ppu->cycle++;
    if (ppu->cycle > 340) {
        ppu->cycle = 0;
        ppu->scanline++;
        if (ppu->scanline > 260) {
            ppu->scanline = -1;
            ppu->frame++;
        }
    }

    return nmi_triggered;
}

/* -----------------------------------------------------------------------
 * ppu_init / ppu_reset
 * ----------------------------------------------------------------------- */
void ppu_init(ppu_t *ppu, nes_t *nes)
{
    memset(ppu, 0, sizeof(*ppu));
    ppu->nes      = nes;
    ppu->scanline = -1;
}

void ppu_reset(ppu_t *ppu)
{
    nes_t *nes = ppu->nes;   /* Preserve the back-pointer */

    ppu->ctrl      = 0;
    ppu->mask      = 0;
    ppu->status    = 0;
    ppu->oam_addr  = 0;
    ppu->v         = 0;
    ppu->t         = 0;
    ppu->fine_x    = 0;
    ppu->w         = false;
    ppu->data_buf  = 0;
    ppu->scanline  = -1;
    ppu->cycle     = 0;
    ppu->frame     = 0;
    ppu->nmi_occurred = false;
    ppu->nmi_output   = false;

    memset(ppu->nametable,   0, sizeof(ppu->nametable));
    memset(ppu->palette,     0, sizeof(ppu->palette));
    memset(ppu->oam,         0, sizeof(ppu->oam));
    memset(ppu->framebuffer, 0, sizeof(ppu->framebuffer));

    ppu->nes = nes;
}
