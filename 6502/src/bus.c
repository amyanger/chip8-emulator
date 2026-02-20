#include "bus.h"
#include <stdio.h>
#include <string.h>

void bus_flat_init(bus_flat_t *bus)
{
    memset(bus->ram, 0, sizeof(bus->ram));
}

bool bus_flat_load(bus_flat_t *bus, const char *path, uint16_t base_addr)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "bus_flat_load: cannot open '%s'\n", path);
        return false;
    }

    /* Determine file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size < 0) {
        fprintf(stderr, "bus_flat_load: cannot determine size of '%s'\n", path);
        fclose(f);
        return false;
    }

    /* Validate that the binary fits within the 64KB address space */
    if ((long)base_addr + file_size > 65536) {
        fprintf(stderr, "bus_flat_load: file '%s' (%ld bytes) at base $%04X "
                "exceeds 64KB address space\n", path, file_size, base_addr);
        fclose(f);
        return false;
    }

    size_t read = fread(bus->ram + base_addr, 1, (size_t)file_size, f);
    if ((long)read != file_size) {
        fprintf(stderr, "bus_flat_load: short read on '%s' "
                "(expected %ld, got %zu)\n", path, file_size, read);
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

uint8_t bus_flat_read(void *ctx, uint16_t addr)
{
    bus_flat_t *bus = (bus_flat_t *)ctx;
    return bus->ram[addr];
}

void bus_flat_write(void *ctx, uint16_t addr, uint8_t val)
{
    bus_flat_t *bus = (bus_flat_t *)ctx;
    bus->ram[addr] = val;
}
