#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MIRROR_HORIZONTAL,
    MIRROR_VERTICAL
} mirror_mode_t;

typedef struct {
    uint8_t *prg_rom;
    uint8_t *chr_rom;
    uint8_t  chr_ram[8192];
    uint8_t  prg_banks;      /* number of 16KB PRG banks */
    uint8_t  chr_banks;      /* number of 8KB CHR banks (0 = use chr_ram) */
    uint8_t  mapper_id;
    mirror_mode_t mirror;
} cartridge_t;

bool cartridge_load(cartridge_t *cart, const char *path);
void cartridge_free(cartridge_t *cart);

uint8_t cartridge_cpu_read(cartridge_t *cart, uint16_t addr);
void    cartridge_cpu_write(cartridge_t *cart, uint16_t addr, uint8_t val);

uint8_t cartridge_chr_read(cartridge_t *cart, uint16_t addr);
void    cartridge_chr_write(cartridge_t *cart, uint16_t addr, uint8_t val);

#endif
