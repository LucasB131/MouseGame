#pragma once

#include <string>
#include <vector>

#include "CatTypes.h"
#include "RoomTheme.h"
#include "raylib.h"

enum class Tile { Floor, Wall, Exit, Hole, Furniture };

// Solid furniture characters in the level grid. They block movement and sight just like walls.
//   B shelf   T table   K counter/cabinet   A appliance   S sofa/seats   D bed   G pool table   U tub/fountain
//   L plant   X boxes/barrels   Q piano   F fireplace   W wardrobe/dresser   Y car
bool IsFurnitureChar(char c);

struct Rug
{
    int x = 0, y = 0, w = 1, h = 1; // in tiles
    Color color{};
};

// A mouse hole: hides the mouse and stores its cheese. Holes with the same digit are a linked pair.
struct MouseHole
{
    Vector2 center{};
    int tileX = 0;
    int tileY = 0;
    int color = 0; // 0..3 (digits 1..4 in the level file)
    int pair = -1; // index of the linked hole, -1 if none
};

constexpr int HoleColorCount = 4;
Color HoleColor(int color);
const char* HoleColorName(int color);

struct CatSpawn
{
    CatKind kind = CatKind::Tabby;
    std::vector<Vector2> route; // waypoints in world (pixel) coordinates
};

// A grid of tiles loaded from a plain-text file.
//   '#' wall   '.' floor   'P' player start   'C' cheese   'E' exit
//   '1'-'4' mouse holes (two holes with the same digit are linked)   'R' red pepper (speed boost)
//   furniture letters (see IsFurnitureChar)
// After the grid and a blank line:
//   name <text>                              the level's display name
//   room <theme>                             kitchen, library, billiards, ... (see RoomTheme.cpp)
//   rug x,y w,h <color>                      decorative rug (walkable), in tiles
//   cat [tabby|sleepy|hunter|blind] x,y ...  a cat and its patrol route (tile coordinates)
//   # ...                                    comment
class Level
{
public:
    static constexpr int TileSize = 40; // pixels per tile

    bool LoadFromFile(const std::string& path);

    int Width() const { return width_; }
    int Height() const { return height_; }

    // Anything outside the grid counts as a wall, so nothing can leave the map.
    Tile At(int x, int y) const;
    bool IsWall(int x, int y) const
    {
        const Tile t = At(x, y);
        return t == Tile::Wall || t == Tile::Furniture;
    }
    char FurnitureAt(int x, int y) const; // furniture letter, or 0
    bool IsWallAt(Vector2 worldPos) const;
    bool CircleOverlapsWall(Vector2 center, float radius) const;

    Rectangle TileRect(int x, int y) const;
    Vector2 PlayerStart() const { return playerStart_; }
    const std::vector<Vector2>& CheeseSpawns() const { return cheeseSpawns_; }
    const std::vector<Vector2>& PepperSpawns() const { return pepperSpawns_; }
    const std::vector<CatSpawn>& Cats() const { return cats_; }
    const std::vector<MouseHole>& Holes() const { return holes_; }
    int HoleAt(int x, int y) const; // index into Holes(), or -1
    const std::string& Name() const { return name_; }
    RoomTheme Theme() const { return theme_; }
    const std::vector<Rug>& Rugs() const { return rugs_; }

    // Distance a ray travels from origin along dir (unit vector) before hitting a wall, capped at maxDist.
    float Raycast(Vector2 origin, Vector2 dir, float maxDist) const;
    bool HasLineOfSight(Vector2 from, Vector2 to) const;

private:
    bool ParseCatRoute(const std::string& line);
    void ParseRug(const std::string& line);

    int width_ = 0;
    int height_ = 0;
    std::vector<Tile> tiles_; // row-major: tiles_[y * width_ + x]
    Vector2 playerStart_{};
    std::vector<Vector2> cheeseSpawns_;
    std::vector<Vector2> pepperSpawns_;
    std::vector<CatSpawn> cats_;
    std::vector<MouseHole> holes_;
    std::vector<char> furniture_; // furniture letter per tile (0 = none)
    std::vector<Rug> rugs_;
    RoomTheme theme_ = RoomTheme::Living;
    std::string name_;
};
