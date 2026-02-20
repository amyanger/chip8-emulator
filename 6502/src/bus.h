#ifndef BUS_H
#define BUS_H

#include <stdint.h>
#include <stdbool.h>

/* Memory bus function pointer types.
 * The CPU accesses memory exclusively through these.
 * For standalone use, bus_flat provides a simple 64KB RAM.
 * For NES integration, replace with a bus that routes to PPU/APU/mappers. */
typedef uint8_t (*bus_read_fn)(void *ctx, uint16_t addr);
typedef void (*bus_write_fn)(void *ctx, uint16_t addr, uint8_t val);

/* Simple flat 64KB memory bus for standalone operation and testing */
typedef struct {
    uint8_t ram[65536];
} bus_flat_t;

void bus_flat_init(bus_flat_t *bus);
bool bus_flat_load(bus_flat_t *bus, const char *path, uint16_t base_addr);

/* These match the bus_read_fn / bus_write_fn signatures */
uint8_t bus_flat_read(void *ctx, uint16_t addr);
void bus_flat_write(void *ctx, uint16_t addr, uint8_t val);

#endif
