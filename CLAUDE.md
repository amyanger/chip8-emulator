# CHIP-8 Emulator

## Project Purpose

A CHIP-8 emulator built as a learning project toward understanding how Nintendo embedded a NES emulator inside Animal Crossing (GameCube). The long-term goal is to contribute to or learn from the [Animal Crossing GameCube decompilation](https://github.com/ACreTeam/ac-decomp) — a C-based reverse engineering effort to reconstruct the original game binary, including its embedded NES emulator.

## Roadmap

1. **CHIP-8 emulator** (current) — learn fetch-decode-execute, timing, memory-mapped I/O, sprite rendering
2. **Standalone 6502 CPU emulator** — the NES CPU; ~150 opcode variants with addressing modes. Test against Klaus Dormann's 6502 functional test suite
3. **NES emulator** — add PPU, APU, and mapper support on top of the 6502 core
4. **Study ac-decomp** — read the reconstructed NES emulator source inside Animal Crossing to understand Nintendo's approach (src/ in the decomp repo, primarily C targeting GameCube/Dolphin)

## Tech Stack

- **Language:** C99
- **Graphics:** SDL2
- **Build:** Make (`make` to build, `make clean` to clean)
- **Platform:** macOS (primary), should work on Linux with SDL2 installed

## Project Structure

```
src/
  chip8.h / chip8.c   — core emulator (CPU, memory, opcodes, timers)
  platform.h / platform.c — SDL2 windowing, rendering, input handling
  main.c               — main loop, timing (500Hz CPU, 60Hz timers)
Makefile               — single-step build, no intermediate .o files
```

## Conventions

- Keep the core emulator (`chip8.c`) free of platform dependencies (no SDL includes)
- Use defensive bounds checking: mask memory access with `& 0xFFF`, check PC/stack bounds
- Compute flags (VF) before overwriting registers in arithmetic opcodes
- Prefer `fprintf(stderr, ...)` for error reporting, no `exit()` inside library code
- No external dependencies beyond SDL2 and the C standard library

## Known Quirks / Compatibility

The emulator uses modern/SCHIP behavior for ambiguous instructions:
- `8XY6`/`8XYE`: shifts VX directly (original VIP shifts VY into VX)
- `FX55`/`FX65`: leaves I unchanged (original VIP increments I)

## Git

- Do not include `Co-Authored-By` lines in commits. The sole contributor is Arjun.

## Running

```
make
./chip8 <rom-file>
```

Window scale is 10x (640x320). Keypad maps 1-4/Q-R/A-F/Z-V to the CHIP-8 hex keypad.
