#pragma once

#include <deque>

#include "raylib.h"

// Remembers where the mouse has been so carried cheese can follow behind it like a conga line.
class CheeseTrail
{
public:
    void Reset(Vector2 head);
    void Record(Vector2 head); // call every frame with the mouse's position

    // The point `distance` pixels back along the path the mouse walked.
    Vector2 PointBehind(Vector2 head, float distance) const;

private:
    std::deque<Vector2> points_; // newest first
    static constexpr float MinStep = 3.0f;
    static constexpr size_t MaxPoints = 800;
};
