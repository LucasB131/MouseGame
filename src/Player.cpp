#include "Player.h"

#include <cmath>

#include "Level.h"
#include "Sprites.h"
#include "raymath.h"

Player::Player(Vector2 start) : pos_(start) {}

bool Player::MovementKeyDown()
{
    return IsKeyDown(KEY_W) || IsKeyDown(KEY_UP) || IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN) ||
           IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);
}

void Player::Update(const Level& level, float dt)
{
    Vector2 dir{0.0f, 0.0f};
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    dir.y -= 1.0f;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  dir.y += 1.0f;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  dir.x -= 1.0f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) dir.x += 1.0f;

    moving_ = !(dir.x == 0.0f && dir.y == 0.0f);
    if (!moving_) return;

    dir = Vector2Normalize(dir);
    facing_ = dir;
    const Vector2 delta = Vector2Scale(dir, speed_ * speedMultiplier_ * dt);

    // Move one axis at a time so the player slides along walls instead of sticking.
    const Vector2 before = pos_;
    pos_.x += delta.x;
    ResolveWallCollisions(level);
    pos_.y += delta.y;
    ResolveWallCollisions(level);
    walkPhase_ += Vector2Distance(before, pos_) * 0.35f;
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

void Player::Draw(bool flashRed, MouseSkin skin) const
{
    MouseLook look;
    look.pos = pos_;
    look.facing = facing_;
    look.walkPhase = walkPhase_;
    look.moving = moving_;
    look.flashRed = flashRed;
    look.skin = skin;
    DrawMouseSprite(look);
}
