# MouseGame (working title)

A top-down stealth game written in C++20 with [raylib](https://www.raylib.com/).
Sneak a mouse past patrolling cats, grab the cheese, and reach the exit.

## Build (Windows, MSYS2 UCRT64)

Requirements: `gcc`, `cmake`, `ninja`, `gdb` (from MSYS2 UCRT64) and Git on your PATH.

```bash
cmake --preset debug
cmake --build --preset debug
./build/debug/MouseGame.exe
```

In VS Code (C/C++ + CMake Tools extensions): open the folder, choose the **debug** preset, then press **F5**.

raylib is downloaded automatically by CMake on the first configure.

## Controls

| Key | Action |
|-----|--------|
| WASD / Arrows | Move |
| R | Restart level |
| Esc | Quit |

## Level format

Levels live in `assets/levels/` as plain text, one character per 40x40 tile:

| Char | Meaning |
|------|---------|
| `#` | Wall |
| `.` | Floor |
| `P` | Player start |
| `C` | Cheese |
| `E` | Exit (opens once all cheese is collected) |

## Code layout

- `src/Level.*`: loads and draws the tile grid
- `src/Player.*`: movement and circle-vs-tile collision
- `src/Game.*`: game state, cheese, win condition, HUD
- `src/main.cpp`: window setup and main loop

## Roadmap

- [x] Window, player movement, collectible
- [x] Tile-based levels loaded from text files, wall collision
- [x] Cheese collection, locked exit, level-complete screen
- [ ] Cat enemies with patrol routes and vision cones
- [ ] Detection, chase, and fail state
- [ ] Level exit, scoring, multiple levels
