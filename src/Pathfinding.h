#pragma once

#include <vector>

#include "raylib.h"

class Level;

// A* search over the level's tile grid (8 directions, never cutting across wall corners).
// Returns world-space waypoints leading from `from` to the center of `to`'s tile, excluding the
// starting point. The path is smoothed so an agent of the given radius walks straight lines where
// it safely can instead of zig-zagging tile to tile. Empty if the goal is unreachable.
std::vector<Vector2> FindPath(const Level& level, Vector2 from, Vector2 to, float agentRadius);

// True if a circle of `radius` can slide in a straight line from a to b without touching a wall.
bool HasClearance(const Level& level, Vector2 a, Vector2 b, float radius);
