# CHIP-8 Emulator

A CHIP-8 emulator written in C with SDL2 for display and input.

CHIP-8 is an interpreted programming language from the 1970s, originally used on the COSMAC VIP and Telmac 1800 microcomputers. It has become the classic first emulator project due to its simplicity: 35 instructions, 4KB of memory, and a 64x32 monochrome display.

## Building

### Dependencies

- GCC (or any C99 compiler)
- SDL2
- Make

Install SDL2:

```bash
# macOS
brew install sdl2

# Ubuntu/Debian
sudo apt install libsdl2-dev

# Arch
sudo pacman -S sdl2
```

### Compile

```bash
make
```

### Run

```bash
./chip8 <path-to-rom>
```

## Controls

The CHIP-8 has a 16-key hex keypad. Keys are mapped to your keyboard as follows:

```
CHIP-8 Keypad        Keyboard
+-+-+-+-+            +-+-+-+-+
|1|2|3|C|            |1|2|3|4|
+-+-+-+-+            +-+-+-+-+
|4|5|6|D|            |Q|W|E|R|
+-+-+-+-+            +-+-+-+-+
|7|8|9|E|            |A|S|D|F|
+-+-+-+-+            +-+-+-+-+
|A|0|B|F|            |Z|X|C|V|
+-+-+-+-+            +-+-+-+-+
```

Press **Escape** to quit.

## Architecture

```
src/
  chip8.h       -- CHIP-8 state: memory, registers, display, timers
  chip8.c       -- CPU: fetch-decode-execute loop, all 35 instructions
  platform.h    -- SDL2 abstraction for display and input
  platform.c    -- Window creation, rendering, keyboard handling
  main.c        -- Entry point, main loop with timing
```

The emulator runs the CPU at 500 Hz and ticks the delay/sound timers at 60 Hz.

## CHIP-8 Specs

| Component | Details |
|---|---|
| Memory | 4 KB (4096 bytes) |
| Registers | 16 general purpose (V0-VF), VF is the flag register |
| Index Register | 16-bit, used for memory addresses |
| Program Counter | Starts at 0x200 |
| Stack | 16 levels |
| Display | 64x32 pixels, monochrome |
| Timers | Delay timer, sound timer (both count down at 60 Hz) |
| Input | 16-key hex keypad |
| Instructions | 35 opcodes, each 2 bytes |

## ROMs

CHIP-8 ROMs are widely available online. Search for "chip-8 roms" or "chip-8 test suite" to find public domain games and test programs. Some classics:

- **Pong** - the classic
- **Space Invaders**
- **Tetris**
- **Brix** (Breakout clone)

The [Timendus CHIP-8 test suite](https://github.com/Timendus/chip8-test-suite) is useful for verifying emulator correctness.

## License

MIT
