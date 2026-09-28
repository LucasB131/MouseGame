#pragma once

#include <string>
#include <vector>

#include "raylib.h"

enum class Tile { Floor, Wall, Exit };

// A grid of tiles loaded from a plain-text file.
//   '#' wall   '.' floor   'P' player start   'C' cheese   'E' exit
class Level
{
public:
    static constexpr int TileSize = 40; // pixels per tile

    bool LoadFromFile(const std::string& path);

    int Width() const { return width_; }
    int Height() const { return height_; }

    // Anything outside the grid counts as a wall, so the player can never leave the map.
    Tile At(int x, int y) const;
    bool IsWall(int x, int y) const { return At(x, y) == Tile::Wall; }

    Rectangle TileRect(int x, int y) const;
    Vector2 PlayerStart() const { return playerStart_; }
    const std::vector<Vector2>& CheeseSpawns() const { return cheeseSpawns_; }

    void Draw(bool exitOpen) const;

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<Tile> tiles_; // row-major: tiles_[y * width_ + x]
    Vector2 playerStart_{};
    std::vector<Vector2> cheeseSpawns_;
};
