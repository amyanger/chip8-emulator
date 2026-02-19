#ifndef PLATFORM_H
#define PLATFORM_H

#include <SDL.h>
#include <stdbool.h>

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
} platform_t;

bool platform_init(platform_t *plat, const char *title, int scale);
void platform_destroy(platform_t *plat);
void platform_render(platform_t *plat, const uint8_t *display, int width, int height);
bool platform_handle_input(uint8_t *keypad);

#endif
