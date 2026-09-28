#include "Level.h"

#include <algorithm>
#include <fstream>

namespace
{
Vector2 TileCenter(int x, int y)
{
    return {(x + 0.5f) * Level::TileSize, (y + 0.5f) * Level::TileSize};
}
} // namespace

bool Level::LoadFromFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file) return false;

    std::vector<std::string> rows;
    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back(); // handle Windows line endings
        rows.push_back(line);
    }
    while (!rows.empty() && rows.back().empty()) rows.pop_back();
    if (rows.empty()) return false;

    height_ = static_cast<int>(rows.size());
    width_ = 0;
    for (const auto& r : rows) width_ = std::max(width_, static_cast<int>(r.size()));

    tiles_.assign(static_cast<size_t>(width_) * height_, Tile::Wall); // short rows are padded with walls
    cheeseSpawns_.clear();
    playerStart_ = TileCenter(1, 1);

    for (int y = 0; y < height_; ++y)
    {
        for (int x = 0; x < static_cast<int>(rows[y].size()); ++x)
        {
            Tile& tile = tiles_[y * width_ + x];
            switch (rows[y][x])
            {
            case '#': tile = Tile::Wall; break;
            case 'E': tile = Tile::Exit; break;
            case 'P': tile = Tile::Floor; playerStart_ = TileCenter(x, y); break;
            case 'C': tile = Tile::Floor; cheeseSpawns_.push_back(TileCenter(x, y)); break;
            default:  tile = Tile::Floor; break;
            }
        }
    }
    return true;
}

Tile Level::At(int x, int y) const
{
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return Tile::Wall;
    return tiles_[y * width_ + x];
}

Rectangle Level::TileRect(int x, int y) const
{
    return {static_cast<float>(x * TileSize), static_cast<float>(y * TileSize),
            static_cast<float>(TileSize), static_cast<float>(TileSize)};
}

void Level::Draw(bool exitOpen) const
{
    const Color floorA{58, 52, 48, 255};
    const Color floorB{64, 57, 52, 255};
    const Color wallFace{92, 70, 52, 255};
    const Color wallTop{128, 100, 74, 255};

    for (int y = 0; y < height_; ++y)
    {
        for (int x = 0; x < width_; ++x)
        {
            const Rectangle r = TileRect(x, y);
            switch (At(x, y))
            {
            case Tile::Floor:
                DrawRectangleRec(r, ((x + y) % 2 == 0) ? floorA : floorB);
                break;
            case Tile::Wall:
                DrawRectangleRec(r, wallFace);
                DrawRectangle(static_cast<int>(r.x), static_cast<int>(r.y), TileSize, 6, wallTop);
                break;
            case Tile::Exit:
                DrawRectangleRec(r, floorA);
                DrawCircle(static_cast<int>(r.x + TileSize / 2), static_cast<int>(r.y + TileSize / 2),
                           TileSize * 0.38f, exitOpen ? Color{60, 200, 110, 255} : Color{25, 25, 25, 255});
                if (!exitOpen)
                    DrawRectangleLinesEx(r, 2.0f, Color{150, 60, 60, 255}); // locked
                break;
            }
        }
    }
}
