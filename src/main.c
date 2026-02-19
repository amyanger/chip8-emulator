#include "chip8.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: chip8 <rom>\n");
        return 1;
    }

    chip8_t chip;
    chip8_init(&chip);

    if (!chip8_load_rom(&chip, argv[1])) {
        return 1;
    }

    // TODO: main loop with display and input

    return 0;
}
