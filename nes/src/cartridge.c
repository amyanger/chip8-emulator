#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cartridge.h"

#define INES_HEADER_SIZE  16
#define PRG_BANK_SIZE     16384   /* 16KB */
#define CHR_BANK_SIZE     8192    /* 8KB  */
#define TRAINER_SIZE      512

static const uint8_t ines_magic[4] = {0x4E, 0x45, 0x53, 0x1A};

bool cartridge_load(cartridge_t *cart, const char *path)
{
    uint8_t header[INES_HEADER_SIZE];
    FILE *fp = NULL;

    if (!cart || !path) {
        fprintf(stderr, "cartridge_load: NULL argument\n");
        return false;
    }

    /* Zero out the struct so cleanup is safe on any error path */
    memset(cart, 0, sizeof(*cart));

    fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "cartridge_load: cannot open '%s'\n", path);
        return false;
    }

    /* --- Read and validate the 16-byte iNES header --- */
    if (fread(header, 1, INES_HEADER_SIZE, fp) != INES_HEADER_SIZE) {
        fprintf(stderr, "cartridge_load: failed to read iNES header\n");
        goto fail;
    }

    if (memcmp(header, ines_magic, 4) != 0) {
        fprintf(stderr, "cartridge_load: invalid iNES magic bytes\n");
        goto fail;
    }

    /* --- Extract fields from the header --- */
    cart->prg_banks = header[4];
    cart->chr_banks = header[5];
    cart->mapper_id = (header[7] & 0xF0) | (header[6] >> 4);
    cart->mirror    = (header[6] & 0x01) ? MIRROR_VERTICAL : MIRROR_HORIZONTAL;

    if (cart->mapper_id != 0) {
        fprintf(stderr, "Unsupported mapper: %d\n", cart->mapper_id);
        goto fail;
    }

    if (cart->prg_banks == 0) {
        fprintf(stderr, "cartridge_load: PRG bank count is 0\n");
        goto fail;
    }

    /* --- Skip trainer if present (512 bytes) --- */
    if (header[6] & 0x04) {
        if (fseek(fp, TRAINER_SIZE, SEEK_CUR) != 0) {
            fprintf(stderr, "cartridge_load: failed to skip trainer\n");
            goto fail;
        }
    }

    /* --- Load PRG ROM --- */
    size_t prg_size = (size_t)cart->prg_banks * PRG_BANK_SIZE;
    cart->prg_rom = malloc(prg_size);
    if (!cart->prg_rom) {
        fprintf(stderr, "cartridge_load: failed to allocate PRG ROM (%zu bytes)\n",
                prg_size);
        goto fail;
    }

    if (fread(cart->prg_rom, 1, prg_size, fp) != prg_size) {
        fprintf(stderr, "cartridge_load: failed to read PRG ROM\n");
        goto fail;
    }

    /* --- Load CHR ROM or initialize CHR RAM --- */
    if (cart->chr_banks > 0) {
        size_t chr_size = (size_t)cart->chr_banks * CHR_BANK_SIZE;
        cart->chr_rom = malloc(chr_size);
        if (!cart->chr_rom) {
            fprintf(stderr, "cartridge_load: failed to allocate CHR ROM (%zu bytes)\n",
                    chr_size);
            goto fail;
        }

        if (fread(cart->chr_rom, 1, chr_size, fp) != chr_size) {
            fprintf(stderr, "cartridge_load: failed to read CHR ROM\n");
            goto fail;
        }
    } else {
        /* CHR banks == 0 means the board has CHR RAM instead */
        cart->chr_rom = NULL;
        memset(cart->chr_ram, 0, sizeof(cart->chr_ram));
    }

    fclose(fp);
    return true;

fail:
    if (fp)
        fclose(fp);
    cartridge_free(cart);
    return false;
}

void cartridge_free(cartridge_t *cart)
{
    if (!cart)
        return;

    free(cart->prg_rom);
    cart->prg_rom = NULL;

    free(cart->chr_rom);
    cart->chr_rom = NULL;
}

/* ---------------------------------------------------------------------------
 * CPU bus read  ($4020-$FFFF mapped through the cartridge)
 * Mapper 0 (NROM) layout:
 *   $6000-$7FFF  PRG RAM  — not present on NROM, return 0
 *   $8000-$BFFF  first 16KB PRG bank
 *   $C000-$FFFF  last 16KB PRG bank (mirrors first if only 1 bank)
 * ------------------------------------------------------------------------- */
uint8_t cartridge_cpu_read(cartridge_t *cart, uint16_t addr)
{
    if (addr >= 0x8000) {
        if (addr < 0xC000) {
            /* $8000-$BFFF: first PRG bank */
            return cart->prg_rom[addr & 0x3FFF];
        }
        /* $C000-$FFFF: last PRG bank, mirror if only 1 bank */
        if (cart->prg_banks == 1)
            return cart->prg_rom[addr & 0x3FFF];
        return cart->prg_rom[0x4000 + (addr & 0x3FFF)];
    }

    /* $6000-$7FFF: PRG RAM — NROM has none */
    /* $4020-$5FFF: expansion area — nothing mapped */
    return 0;
}

/* ---------------------------------------------------------------------------
 * CPU bus write — Mapper 0 has no writable registers, so this is a no-op.
 * ------------------------------------------------------------------------- */
void cartridge_cpu_write(cartridge_t *cart, uint16_t addr, uint8_t val)
{
    (void)cart;
    (void)addr;
    (void)val;
}

/* ---------------------------------------------------------------------------
 * PPU / CHR bus read  ($0000-$1FFF)
 * ------------------------------------------------------------------------- */
uint8_t cartridge_chr_read(cartridge_t *cart, uint16_t addr)
{
    if (cart->chr_banks > 0)
        return cart->chr_rom[addr & 0x1FFF];
    return cart->chr_ram[addr & 0x1FFF];
}

/* ---------------------------------------------------------------------------
 * PPU / CHR bus write — only effective when using CHR RAM (chr_banks == 0).
 * Writes to CHR ROM are silently ignored.
 * ------------------------------------------------------------------------- */
void cartridge_chr_write(cartridge_t *cart, uint16_t addr, uint8_t val)
{
    if (cart->chr_banks == 0)
        cart->chr_ram[addr & 0x1FFF] = val;
}
