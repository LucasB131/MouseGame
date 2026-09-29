#include "Level.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace
{
Vector2 TileCenter(int x, int y)
{
    return {(x + 0.5f) * Level::TileSize, (y + 0.5f) * Level::TileSize};
}
} // namespace

bool IsFurnitureChar(char c)
{
    switch (c)
    {
    case 'B': case 'T': case 'K': case 'A': case 'S': case 'D': case 'G': case 'U':
    case 'L': case 'X': case 'Q': case 'F': case 'W': case 'Y':
    case 'N': case 'M': case 'V':
        return true;
    default:
        return false;
    }
}

Color HoleColor(int color)
{
    static const Color colors[HoleColorCount] = {{235, 80, 80, 255}, {80, 150, 245, 255}, {185, 105, 235, 255}, {50, 205, 190, 255}};
    return colors[color % HoleColorCount];
}

const char* HoleColorName(int color)
{
    static const char* names[HoleColorCount] = {"red", "blue", "purple", "teal"};
    return names[color % HoleColorCount];
}

bool Level::LoadFromFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file) return false;

    // The grid is every line up to the first blank line; anything after is route data / comments.
    std::vector<std::string> rows;
    std::vector<std::string> extra;
    std::string line;
    bool inGrid = true;
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back(); // handle Windows line endings
        if (inGrid && line.empty() && !rows.empty())
        {
            inGrid = false;
            continue;
        }
        (inGrid ? rows : extra).push_back(line);
    }
    if (rows.empty()) return false;

    height_ = static_cast<int>(rows.size());
    width_ = 0;
    for (const auto& r : rows) width_ = std::max(width_, static_cast<int>(r.size()));

    tiles_.assign(static_cast<size_t>(width_) * height_, Tile::Wall); // short rows are padded with walls
    furniture_.assign(tiles_.size(), 0);
    rugs_.clear();
    theme_ = RoomTheme::Living;
    cheeseSpawns_.clear();
    cheeseKinds_.clear();
    pepperSpawns_.clear();
    cats_.clear();
    holes_.clear();
    name_.clear();
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
            case 'C':
                tile = Tile::Floor;
                cheeseSpawns_.push_back(TileCenter(x, y));
                cheeseKinds_.push_back(LightCheeseAt(x, y));
                break;
            case 'O':
                tile = Tile::Floor;
                cheeseSpawns_.push_back(TileCenter(x, y));
                cheeseKinds_.push_back(CheeseKind::Gouda);
                break;
            case 'R': tile = Tile::Floor; pepperSpawns_.push_back(TileCenter(x, y)); break;
            case '1': case '2': case '3': case '4':
                tile = Tile::Hole;
                holes_.push_back({TileCenter(x, y), x, y, rows[y][x] - '1', -1});
                break;
            default:
                if (IsFurnitureChar(rows[y][x]))
                {
                    tile = Tile::Furniture;
                    furniture_[y * width_ + x] = rows[y][x];
                }
                else
                {
                    tile = Tile::Floor;
                }
                break;
            }
        }
    }

    // Link holes of the same color in pairs.
    for (int color = 0; color < HoleColorCount; ++color)
    {
        std::vector<int> same;
        for (int i = 0; i < static_cast<int>(holes_.size()); ++i)
            if (holes_[i].color == color) same.push_back(i);
        if (same.size() > 2) TraceLog(LOG_WARNING, "Level: more than two %s holes; only the first two are linked", HoleColorName(color));
        if (same.size() >= 2)
        {
            holes_[same[0]].pair = same[1];
            holes_[same[1]].pair = same[0];
        }
    }

    for (const std::string& l : extra)
    {
        if (l.rfind("name ", 0) == 0)
            name_ = l.substr(5);
        else if (l.rfind("room ", 0) == 0)
        {
            if (!ParseRoomTheme(l.substr(5), theme_)) TraceLog(LOG_WARNING, "Level: unknown room theme: %s", l.c_str());
        }
        else if (l.rfind("rug ", 0) == 0)
        {
            ParseRug(l);
        }
        else if (l.rfind("cat ", 0) == 0 && !ParseCatRoute(l))
            TraceLog(LOG_WARNING, "Level: bad cat route: %s", l.c_str());
    }
    return true;
}

void Level::ParseRug(const std::string& line)
{
    // rug x,y w,h color
    std::istringstream in(line.substr(4));
    Rug rug;
    char comma = 0;
    std::string color;
    if (!(in >> rug.x >> comma >> rug.y >> rug.w >> comma >> rug.h >> color))
    {
        TraceLog(LOG_WARNING, "Level: bad rug: %s", line.c_str());
        return;
    }
    static const std::pair<const char*, Color> colors[] = {
        {"red", {130, 40, 44, 255}}, {"blue", {50, 70, 120, 255}}, {"green", {50, 96, 64, 255}},
        {"gold", {150, 112, 50, 255}}, {"purple", {90, 56, 110, 255}}, {"teal", {40, 104, 104, 255}},
        {"cream", {170, 150, 120, 255}}, {"lapis", {40, 64, 124, 255}}, {"sand", {190, 160, 108, 255}},
        {"onyx", {44, 42, 52, 255}},
    };
    rug.color = colors[0].second;
    for (const auto& [name, c] : colors)
        if (color == name) rug.color = c;
    rugs_.push_back(rug);
}

bool Level::ParseCatRoute(const std::string& line)
{
    std::istringstream in(line.substr(3));
    CatSpawn spawn;
    std::string token;
    bool first = true;
    while (in >> token)
    {
        if (first && ParseCatKind(token, spawn.kind))
        {
            first = false;
            continue;
        }
        first = false;
        int x = 0, y = 0;
        char comma = 0;
        std::istringstream pt(token);
        if (!(pt >> x >> comma >> y) || comma != ',' || IsWall(x, y)) return false;
        spawn.route.push_back(TileCenter(x, y));
    }
    if (spawn.route.empty()) return false;

    const auto& route = spawn.route;
    for (size_t i = 1; i < route.size(); ++i)
    {
        if (!HasLineOfSight(route[i - 1], route[i]))
            TraceLog(LOG_WARNING, "Level: cat route segment %zu is blocked by a wall: %s", i, line.c_str());
    }
    cats_.push_back(std::move(spawn));
    return true;
}

Tile Level::At(int x, int y) const
{
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return Tile::Wall;
    return tiles_[y * width_ + x];
}

char Level::FurnitureAt(int x, int y) const
{
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return 0;
    return furniture_[y * width_ + x];
}

int Level::HoleAt(int x, int y) const
{
    for (int i = 0; i < static_cast<int>(holes_.size()); ++i)
        if (holes_[i].tileX == x && holes_[i].tileY == y) return i;
    return -1;
}

bool Level::IsWallAt(Vector2 p) const
{
    return IsWall(static_cast<int>(std::floor(p.x / TileSize)), static_cast<int>(std::floor(p.y / TileSize)));
}

bool Level::CircleOverlapsWall(Vector2 c, float radius) const
{
    const int minX = static_cast<int>(std::floor((c.x - radius) / TileSize));
    const int maxX = static_cast<int>(std::floor((c.x + radius) / TileSize));
    const int minY = static_cast<int>(std::floor((c.y - radius) / TileSize));
    const int maxY = static_cast<int>(std::floor((c.y + radius) / TileSize));
    for (int y = minY; y <= maxY; ++y)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            if (!IsWall(x, y)) continue;
            const Rectangle r = TileRect(x, y);
            const float dx = c.x - std::clamp(c.x, r.x, r.x + r.width);
            const float dy = c.y - std::clamp(c.y, r.y, r.y + r.height);
            if (dx * dx + dy * dy < radius * radius) return true;
        }
    }
    return false;
}

float Level::Raycast(Vector2 origin, Vector2 dir, float maxDist) const
{
    // March along the ray in small steps. Cheap enough for a few dozen rays per cat per frame.
    constexpr float step = 2.0f;
    for (float t = 0.0f; t < maxDist; t += step)
    {
        if (IsWallAt({origin.x + dir.x * t, origin.y + dir.y * t})) return t;
    }
    return maxDist;
}

bool Level::HasLineOfSight(Vector2 from, Vector2 to) const
{
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) return true;
    return Raycast(from, {dx / len, dy / len}, len) >= len;
}

Rectangle Level::TileRect(int x, int y) const
{
    return {static_cast<float>(x * TileSize), static_cast<float>(y * TileSize),
            static_cast<float>(TileSize), static_cast<float>(TileSize)};
}
