#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>

#include "nes.h"
#include "platform_nes.h"

#define TARGET_FPS     60
#define FRAME_TIME_MS  (1000 / TARGET_FPS)   /* ~16 ms */

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <rom.nes>\n", argv[0]);
        exit(1);
    }

    const char *rom_path = argv[1];

    nes_t nes;
    if (!nes_init(&nes, rom_path)) {
        fprintf(stderr, "Failed to initialize NES with ROM '%s'\n", rom_path);
        exit(1);
    }

    nes_platform_t plat;
    if (!nes_platform_init(&plat, "NES", 3)) {
        fprintf(stderr, "Failed to initialize SDL2 platform\n");
        nes_free(&nes);
        exit(1);
    }

    bool running = true;
    while (running) {
        Uint32 frame_start = SDL_GetTicks();

        /* Poll input and check for quit */
        uint8_t buttons = 0;
        if (!nes_platform_poll_input(&buttons)) {
            running = false;
            break;
        }
        nes_set_controller(&nes, 0, buttons);

        /* Run one full frame of emulation */
        nes_step_frame(&nes);

        /* Render the PPU framebuffer */
        nes_platform_render(&plat, nes.ppu.framebuffer);

        /* Frame timing: delay to maintain ~60 FPS */
        Uint32 frame_elapsed = SDL_GetTicks() - frame_start;
        if (frame_elapsed < FRAME_TIME_MS) {
            SDL_Delay(FRAME_TIME_MS - frame_elapsed);
        }
    }

    nes_platform_destroy(&plat);
    nes_free(&nes);
    return 0;
}
