#ifndef PLATFORM_NES_H
#define PLATFORM_NES_H

#include <stdint.h>
#include <stdbool.h>
#include <SDL.h>

typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    SDL_Texture  *texture;
} nes_platform_t;

bool nes_platform_init(nes_platform_t *plat, const char *title, int scale);
void nes_platform_destroy(nes_platform_t *plat);
void nes_platform_render(nes_platform_t *plat, const uint32_t *framebuffer);
bool nes_platform_poll_input(uint8_t *controller);

#endif
