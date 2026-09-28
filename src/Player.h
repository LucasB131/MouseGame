#pragma once

#include "raylib.h"

class Level;

class Player
{
public:
    explicit Player(Vector2 start = {0.0f, 0.0f});

    void Update(const Level& level, float dt);
    void Draw() const;

    Vector2 Position() const { return pos_; }
    void SetPosition(Vector2 pos) { pos_ = pos; }
    void SetSpeedMultiplier(float m) { speedMultiplier_ = m; }

    static bool MovementKeyDown();
    float Radius() const { return radius_; }

private:
    // Push the player out of any wall tile it overlaps.
    void ResolveWallCollisions(const Level& level);

    Vector2 pos_;
    Vector2 facing_{1.0f, 0.0f}; // unit vector, direction of last movement
    float radius_ = 14.0f;
    float speed_ = 220.0f; // pixels per second
    float speedMultiplier_ = 1.0f; // < 1 while carrying cheese
};
