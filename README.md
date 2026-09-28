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
| Esc | Quit |

## Roadmap

- [x] Window, player movement, collectible
- [ ] Tile-based levels and walls
- [ ] Cat enemies with patrol routes and vision cones
- [ ] Detection, chase, and fail state
- [ ] Level exit, scoring, multiple levels
