# MouseGame (working title)

A top-down stealth game written in C++20 with [raylib](https://www.raylib.com/).
Sneak a mouse past patrolling cats, grab all the cheese, and reach the exit without getting caught.

![Level select](docs/menu.png)
![Gameplay](docs/gameplay.png)

## Features

- **Cheese trail**: picked-up cheese follows behind the mouse in a line, and each piece slows you down (7% each, down to 55% speed)
- **Color-coded mouse holes**: step in to hide from cats and safely store the cheese you're carrying; press Space to travel to the matching hole
- **Dotted patrol paths** show where each cat walks
- **Five hand-built levels** with a level-select homepage, per-level timer, and saved best times
- **Three cat types** with different speed, vision and behavior:
  - **Tabby**: the standard patrol guard
  - **Sleepy**: slow and short-sighted, naps on a cycle; sneak past while it snoozes
  - **Hunter**: fast, long narrow vision cone, recognizes you quickly
- **Vision cones blocked by walls**, built by raycasting through the tile grid
- **Cat AI**: a finite-state machine (patrol, investigate, search, return) with **A\* pathfinding** to the player's last known position
- **Detection meter** that fills faster the closer a cat is, so a distant glimpse only makes a cat suspicious
- **Data-driven levels**: plain text files, no recompiling to add or edit a level

## Build (Windows, MSYS2 UCRT64)

Requirements: `gcc`, `gdb`, `cmake`, `ninja` (from MSYS2 UCRT64) and Git.

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
| Space | In a mouse hole: travel to the matching hole |
| R | Restart level |
| Enter | Next level (after winning) |
| Esc | Back to menu / quit from menu |
| F1 | Debug view: cat routes, A* paths, last known position |

## Cheese and mouse holes

Picking up cheese doesn't bank it: it trails behind you and slows you down. Walk into any mouse hole to store it,
and while you're in a hole the cats can't see or catch you. Press Space to pop out of the matching hole, or press a
direction key to leave. The exit only works once every piece of cheese has been picked up; stepping on it stores the
rest and finishes the level. The trail itself is a breadcrumb list of recent positions (`src/CheeseTrail.*`), and each
cheese is placed a fixed distance back along it.

## How the cats work

Each cat runs a small state machine:

1. **Patrol**: walk the route, pausing and turning at each waypoint (sleepy cats also nap here).
2. **Investigate**: on spotting the mouse, run to its last known position along an A* path.
3. **Search**: sweep its gaze around that spot for a few seconds.
4. **Return**: path back to the patrol route and resume.

A cat sees the mouse if it's within range, inside the cone angle, and a ray from the cat to the mouse doesn't hit a wall.
Detection is a shared meter that fills while any cat can see you (faster up close, and faster for Hunters).

Cat stats (speed, view range, cone angle, detection speed, nap timing) are all in one table in `src/CatTypes.cpp`.

## Level format

Levels live in `assets/levels/level1.txt`, `level2.txt`, and so on. The game loads them in order until a number is
missing, so adding `level6.txt` adds a sixth level to the menu. One character per 40x40 tile:

| Char | Meaning |
|------|---------|
| `#` | Wall |
| `.` | Floor |
| `P` | Player start |
| `C` | Cheese |
| `E` | Exit (usable once every piece of cheese has been picked up) |
| `1`-`4` | Mouse hole; two holes with the same digit are a linked pair (red, blue, purple, teal) |

After the grid, leave a blank line, then:

```
name Nap Time
cat tabby 7,15 29,15
cat sleepy 10,5
cat hunter 2,7 29,7
```

`name` sets the title shown on the menu. Each `cat` line gives a type and patrol waypoints in tile coordinates.
The cat walks them forward, then back. Consecutive waypoints need a clear straight path (the game logs a warning
otherwise). A cat with one waypoint stays put: sleepy cats nap there, others slowly turn in place.

All five levels were checked with an automated solver that runs the real cat code, models the cheese slowdown and
mouse holes, and confirms each level can be beaten without ever being seen.

## Code layout

- `src/main.cpp`: window setup and main loop
- `src/App.*`: homepage/level select, screen flow, saving best times
- `src/Game.*`: one level in play: cheese carrying/storing, mouse holes, detection meter, timer, win/lose, HUD
- `src/Level.*`: loads and draws the tile grid, raycasting / line of sight
- `src/Player.*`: movement and circle-vs-tile collision
- `src/CheeseTrail.*`: position history that carried cheese follows
- `src/Cat.*`: cat behavior state machine, napping, vision cone
- `src/CatTypes.*`: stats for each cat type
- `src/Pathfinding.*`: A* over the tile grid with path smoothing

## Roadmap

- [x] Window, player movement, collectible
- [x] Tile-based levels loaded from text files, wall collision
- [x] Cheese collection, locked exit, level-complete screen
- [x] Cat enemies with patrol routes and wall-blocked vision cones
- [x] Detection meter and caught state
- [x] Cats investigate the player's last known position (A* pathfinding), search, then return to patrol
- [x] Three cat types, five levels, level-select homepage with saved best times
- [ ] Sprites and sound effects
- [x] Mouse holes (hide + teleport), cheese trail with slowdown and storing, dotted patrol paths
- [ ] Noise distractions
