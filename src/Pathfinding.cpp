#include "Pathfinding.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <queue>

#include "Level.h"

namespace
{
constexpr float Sqrt2 = 1.41421356f;

struct OpenNode
{
    float f; // cost so far + estimated cost to goal
    int index;
    bool operator>(const OpenNode& other) const { return f > other.f; }
};

// Admissible heuristic for 8-directional movement: straight steps cost 1, diagonals cost sqrt(2).
float Octile(int dx, int dy)
{
    dx = std::abs(dx);
    dy = std::abs(dy);
    return static_cast<float>(dx + dy) + (Sqrt2 - 2.0f) * static_cast<float>(std::min(dx, dy));
}

int TileCoord(float world) { return static_cast<int>(std::floor(world / Level::TileSize)); }

Vector2 TileCenter(int x, int y) { return {(x + 0.5f) * Level::TileSize, (y + 0.5f) * Level::TileSize}; }
} // namespace

bool HasClearance(const Level& level, Vector2 a, Vector2 b, float radius)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) return true;

    // Check the center line plus two lines offset to either side by the radius.
    const Vector2 n{-dy / len * radius, dx / len * radius};
    return level.HasLineOfSight(a, b) &&
           level.HasLineOfSight({a.x + n.x, a.y + n.y}, {b.x + n.x, b.y + n.y}) &&
           level.HasLineOfSight({a.x - n.x, a.y - n.y}, {b.x - n.x, b.y - n.y});
}

std::vector<Vector2> FindPath(const Level& level, Vector2 from, Vector2 to, float agentRadius)
{
    const int w = level.Width();
    const int h = level.Height();
    const int sx = TileCoord(from.x), sy = TileCoord(from.y);
    const int gx = TileCoord(to.x), gy = TileCoord(to.y);
    if (level.IsWall(gx, gy) || sx < 0 || sy < 0 || sx >= w || sy >= h) return {};

    const int start = sy * w + sx;
    const int goal = gy * w + gx;
    if (start == goal) return {TileCenter(gx, gy)};

    constexpr float inf = std::numeric_limits<float>::infinity();
    std::vector<float> cost(static_cast<size_t>(w) * h, inf); // best known cost from start
    std::vector<int> cameFrom(cost.size(), -1);
    std::vector<bool> closed(cost.size(), false);
    std::priority_queue<OpenNode, std::vector<OpenNode>, std::greater<>> open;

    cost[start] = 0.0f;
    open.push({Octile(gx - sx, gy - sy), start});

    static constexpr int dirs[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

    while (!open.empty())
    {
        const int current = open.top().index;
        open.pop();
        if (closed[current]) continue; // stale duplicate entry
        if (current == goal) break;
        closed[current] = true;

        const int cx = current % w;
        const int cy = current / w;
        for (const auto& d : dirs)
        {
            const int nx = cx + d[0];
            const int ny = cy + d[1];
            if (level.IsWall(nx, ny)) continue;

            const bool diagonal = d[0] != 0 && d[1] != 0;
            // Don't squeeze diagonally between two walls or clip a wall corner.
            if (diagonal && (level.IsWall(cx + d[0], cy) || level.IsWall(cx, cy + d[1]))) continue;

            const int next = ny * w + nx;
            const float newCost = cost[current] + (diagonal ? Sqrt2 : 1.0f);
            if (newCost < cost[next])
            {
                cost[next] = newCost;
                cameFrom[next] = current;
                open.push({newCost + Octile(gx - nx, gy - ny), next});
            }
        }
    }

    if (cameFrom[goal] == -1) return {}; // unreachable

    // Walk back from the goal to rebuild the tile path (excluding the start tile).
    std::vector<Vector2> tiles;
    for (int i = goal; i != start; i = cameFrom[i]) tiles.push_back(TileCenter(i % w, i / w));
    std::reverse(tiles.begin(), tiles.end());

    // Smooth: from each anchor, skip ahead to the farthest tile we can reach in a straight line.
    std::vector<Vector2> path;
    Vector2 anchor = from;
    size_t i = 0;
    while (i < tiles.size())
    {
        size_t j = i;
        while (j + 1 < tiles.size() && HasClearance(level, anchor, tiles[j + 1], agentRadius)) ++j;
        path.push_back(tiles[j]);
        anchor = tiles[j];
        i = j + 1;
    }
    return path;
}
