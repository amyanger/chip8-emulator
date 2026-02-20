#include <stdio.h>

#include "platform_nes.h"
#include "nes.h"

/* ---------------------------------------------------------------------------
 * Initialize the SDL2 platform: window, renderer, and streaming texture.
 * Returns false on failure with a diagnostic message to stderr.
 * ------------------------------------------------------------------------- */
bool nes_platform_init(nes_platform_t *plat, const char *title, int scale)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    int window_width  = NES_WIDTH  * scale;
    int window_height = NES_HEIGHT * scale;

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
        SDL_RENDERER_ACCELERATED);
    if (!plat->renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(plat->window);
        SDL_Quit();
        return false;
    }

    plat->texture = SDL_CreateTexture(plat->renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        NES_WIDTH, NES_HEIGHT);
    if (!plat->texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(plat->renderer);
        SDL_DestroyWindow(plat->window);
        SDL_Quit();
        return false;
    }

    return true;
}

/* ---------------------------------------------------------------------------
 * Tear down SDL2 resources in reverse order of creation.
 * ------------------------------------------------------------------------- */
void nes_platform_destroy(nes_platform_t *plat)
{
    if (plat->texture)  SDL_DestroyTexture(plat->texture);
    if (plat->renderer) SDL_DestroyRenderer(plat->renderer);
    if (plat->window)   SDL_DestroyWindow(plat->window);
    SDL_Quit();
}

/* ---------------------------------------------------------------------------
 * Upload the PPU framebuffer to the GPU texture and present it.
 * framebuffer is 256x240 pixels in ARGB8888 format.
 * ------------------------------------------------------------------------- */
void nes_platform_render(nes_platform_t *plat, const uint32_t *framebuffer)
{
    SDL_UpdateTexture(plat->texture, NULL, framebuffer,
                      NES_WIDTH * (int)sizeof(uint32_t));
    SDL_RenderClear(plat->renderer);
    SDL_RenderCopy(plat->renderer, plat->texture, NULL, NULL);
    SDL_RenderPresent(plat->renderer);
}

/* ---------------------------------------------------------------------------
 * Poll SDL events and build the NES controller byte from keyboard state.
 *
 * Key mapping (player 1 only):
 *   X         -> A      (bit 0)
 *   Z         -> B      (bit 1)
 *   Right Shift -> Select (bit 2)
 *   Return    -> Start  (bit 3)
 *   Up arrow  -> Up     (bit 4)
 *   Down arrow-> Down   (bit 5)
 *   Left arrow-> Left   (bit 6)
 *   Right arrow-> Right (bit 7)
 *
 * Returns false if the user requested quit (SDL_QUIT or Escape).
 * ------------------------------------------------------------------------- */
bool nes_platform_poll_input(uint8_t *controller)
{
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            return false;
        }
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            return false;
        }
    }

    /* Use SDL_GetKeyboardState for held-key support rather than event-based
     * polling. This ensures buttons stay pressed across frames. */
    const Uint8 *keys = SDL_GetKeyboardState(NULL);

    uint8_t buttons = 0;
    if (keys[SDL_SCANCODE_X])      buttons |= BTN_A;
    if (keys[SDL_SCANCODE_Z])      buttons |= BTN_B;
    if (keys[SDL_SCANCODE_RSHIFT]) buttons |= BTN_SELECT;
    if (keys[SDL_SCANCODE_RETURN]) buttons |= BTN_START;
    if (keys[SDL_SCANCODE_UP])     buttons |= BTN_UP;
    if (keys[SDL_SCANCODE_DOWN])   buttons |= BTN_DOWN;
    if (keys[SDL_SCANCODE_LEFT])   buttons |= BTN_LEFT;
    if (keys[SDL_SCANCODE_RIGHT])  buttons |= BTN_RIGHT;

    *controller = buttons;
    return true;
}
