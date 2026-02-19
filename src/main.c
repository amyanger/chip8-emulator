#include "chip8.h"
#include "platform.h"
#include <stdio.h>

/* Target CPU frequency: 500 Hz (500 cycles per second).
 * Timers tick at 60 Hz.
 * We run the loop as fast as possible and use SDL_GetTicks()
 * to schedule cycles and timer ticks at the correct rates.
 */
#define CPU_HZ       500
#define TIMER_HZ     60
#define MS_PER_CYCLE (1000.0 / CPU_HZ)
#define MS_PER_TICK  (1000.0 / TIMER_HZ)

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

    platform_t plat;
    if (!platform_init(&plat, "CHIP-8 Emulator", 10)) {
        fprintf(stderr, "Failed to initialize platform\n");
        return 1;
    }

    bool running = true;
    Uint32 last_cycle_time = SDL_GetTicks();
    Uint32 last_timer_time = SDL_GetTicks();

    while (running) {
        /* Handle input every iteration */
        running = platform_handle_input(chip.keypad);

        Uint32 now = SDL_GetTicks();

        /* Execute CPU cycles at ~500 Hz */
        double cycle_elapsed = (double)(now - last_cycle_time);
        if (cycle_elapsed >= MS_PER_CYCLE) {
            /* Calculate how many cycles we owe; cap to prevent spiral */
            int cycles = (int)(cycle_elapsed / MS_PER_CYCLE);
            if (cycles > 20) {
                cycles = 20;
            }

            for (int i = 0; i < cycles; i++) {
                chip8_cycle(&chip);
            }

            last_cycle_time += (Uint32)(cycles * MS_PER_CYCLE);
        }

        /* Tick timers at 60 Hz */
        double timer_elapsed = (double)(now - last_timer_time);
        if (timer_elapsed >= MS_PER_TICK) {
            chip8_tick_timers(&chip);
            last_timer_time += (Uint32)MS_PER_TICK;
        }

        /* Render when the draw flag is set */
        if (chip.draw_flag) {
            platform_render(&plat, chip.display,
                            CHIP8_DISPLAY_WIDTH, CHIP8_DISPLAY_HEIGHT);
            chip.draw_flag = false;
        }

        /* Small sleep to avoid burning 100% CPU */
        SDL_Delay(1);
    }

    platform_destroy(&plat);
    return 0;
}
