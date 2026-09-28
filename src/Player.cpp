#include "Player.h"

#include <cmath>

#include "Level.h"
#include "raymath.h"

Player::Player(Vector2 start) : pos_(start) {}

void Player::Update(const Level& level, float dt)
{
    Vector2 dir{0.0f, 0.0f};
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    dir.y -= 1.0f;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  dir.y += 1.0f;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  dir.x -= 1.0f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) dir.x += 1.0f;

    if (dir.x == 0.0f && dir.y == 0.0f) return;

    dir = Vector2Normalize(dir);
    facing_ = dir;
    const Vector2 delta = Vector2Scale(dir, speed_ * dt);

    // Move one axis at a time so the player slides along walls instead of sticking.
    pos_.x += delta.x;
    ResolveWallCollisions(level);
    pos_.y += delta.y;
    ResolveWallCollisions(level);
}

void Player::ResolveWallCollisions(const Level& level)
{
    const float ts = static_cast<float>(Level::TileSize);
    const int minX = static_cast<int>(std::floor((pos_.x - radius_) / ts));
    const int maxX = static_cast<int>(std::floor((pos_.x + radius_) / ts));
    const int minY = static_cast<int>(std::floor((pos_.y - radius_) / ts));
    const int maxY = static_cast<int>(std::floor((pos_.y + radius_) / ts));

    for (int y = minY; y <= maxY; ++y)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            if (!level.IsWall(x, y)) continue;

            // Closest point on the wall rectangle to the circle's center.
            const Rectangle r = level.TileRect(x, y);
            const Vector2 closest{Clamp(pos_.x, r.x, r.x + r.width), Clamp(pos_.y, r.y, r.y + r.height)};
            const Vector2 diff = Vector2Subtract(pos_, closest);
            const float dist = Vector2Length(diff);

            // Overlapping: push the center back out along the collision normal.
            if (dist < radius_ && dist > 0.0001f)
                pos_ = Vector2Add(closest, Vector2Scale(diff, radius_ / dist));
        }
    }
}

void Player::Draw() const
{
    const Vector2 side{-facing_.y, facing_.x};
    const Vector2 earBase = Vector2Add(pos_, Vector2Scale(facing_, 4.0f));
    const Vector2 tailBase = Vector2Subtract(pos_, Vector2Scale(facing_, radius_));

    DrawLineEx(tailBase, Vector2Subtract(tailBase, Vector2Scale(facing_, 14.0f)), 3.0f, PINK);   // tail
    DrawCircleV(pos_, radius_, LIGHTGRAY);                                                       // body
    DrawCircleV(Vector2Add(earBase, Vector2Scale(side, 10.0f)), 6.0f, PINK);                     // ears
    DrawCircleV(Vector2Subtract(earBase, Vector2Scale(side, 10.0f)), 6.0f, PINK);
    DrawCircleV(Vector2Add(pos_, Vector2Scale(facing_, radius_)), 3.5f, Color{60, 40, 40, 255}); // nose
}
