#include "platform.h"
#include <string.h>

bool platform_init(platform_t *plat, const char *title, int scale) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    int window_width = 64 * scale;
    int window_height = 32 * scale;

    plat->window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        window_width,
        window_height,
        SDL_WINDOW_SHOWN
    );
    if (!plat->window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    plat->renderer = SDL_CreateRenderer(plat->window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!plat->renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(plat->window);
        SDL_Quit();
        return false;
    }

    plat->texture = SDL_CreateTexture(plat->renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        64, 32);
    if (!plat->texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(plat->renderer);
        SDL_DestroyWindow(plat->window);
        SDL_Quit();
        return false;
    }

    return true;
}

void platform_destroy(platform_t *plat) {
    if (plat->texture)  SDL_DestroyTexture(plat->texture);
    if (plat->renderer) SDL_DestroyRenderer(plat->renderer);
    if (plat->window)   SDL_DestroyWindow(plat->window);
    SDL_Quit();
}

void platform_render(platform_t *plat, const uint8_t *display, int width, int height) {
    /* Convert the 1-bit-per-byte display buffer into RGBA8888 pixels.
     * Pixel on  (1) = white (0xFFFFFFFF)
     * Pixel off (0) = black (0x000000FF)
     *
     * RGBA8888 byte order in a uint32: 0xRRGGBBAA
     */
    uint32_t pixels[64 * 32];
    for (int i = 0; i < width * height; i++) {
        pixels[i] = display[i] ? 0xFFFFFFFF : 0x000000FF;
    }

    SDL_UpdateTexture(plat->texture, NULL, pixels, (int)(width * sizeof(uint32_t)));
    SDL_RenderClear(plat->renderer);
    SDL_RenderCopy(plat->renderer, plat->texture, NULL, NULL);
    SDL_RenderPresent(plat->renderer);
}

bool platform_handle_input(uint8_t *keypad) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            return false;
        }

        if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            uint8_t value = (event.type == SDL_KEYDOWN) ? 1 : 0;

            /*
             * Standard CHIP-8 keypad mapping:
             *
             * CHIP-8 keypad:     Keyboard:
             * 1 2 3 C            1 2 3 4
             * 4 5 6 D            Q W E R
             * 7 8 9 E            A S D F
             * A 0 B F            Z X C V
             */
            switch (event.key.keysym.sym) {
                case SDLK_1: keypad[0x1] = value; break;
                case SDLK_2: keypad[0x2] = value; break;
                case SDLK_3: keypad[0x3] = value; break;
                case SDLK_4: keypad[0xC] = value; break;

                case SDLK_q: keypad[0x4] = value; break;
                case SDLK_w: keypad[0x5] = value; break;
                case SDLK_e: keypad[0x6] = value; break;
                case SDLK_r: keypad[0xD] = value; break;

                case SDLK_a: keypad[0x7] = value; break;
                case SDLK_s: keypad[0x8] = value; break;
                case SDLK_d: keypad[0x9] = value; break;
                case SDLK_f: keypad[0xE] = value; break;

                case SDLK_z: keypad[0xA] = value; break;
                case SDLK_x: keypad[0x0] = value; break;
                case SDLK_c: keypad[0xB] = value; break;
                case SDLK_v: keypad[0xF] = value; break;

                case SDLK_ESCAPE:
                    return false;

                default:
                    break;
            }
        }
    }

    return true;
}
