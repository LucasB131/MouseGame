#include "Cat.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "Level.h"
#include "Pathfinding.h"
#include "raymath.h"

namespace
{
// Wrap an angle into [-PI, PI].
float WrapAngle(float a)
{
    while (a > PI) a -= 2.0f * PI;
    while (a < -PI) a += 2.0f * PI;
    return a;
}

Vector2 FromAngle(float a) { return {std::cos(a), std::sin(a)}; }

float AngleTo(Vector2 from, Vector2 to) { return std::atan2(to.y - from.y, to.x - from.x); }

// raylib only draws triangles whose vertices are in counter-clockwise order; fix the order if needed.
void DrawTriangleAnyOrder(Vector2 a, Vector2 b, Vector2 c, Color color)
{
    const float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross > 0.0f) std::swap(b, c);
    DrawTriangle(a, b, c, color);
}
} // namespace

Cat::Cat(CatKind kind, std::vector<Vector2> route) : stats_(&GetCatStats(kind)), route_(std::move(route))
{
    pos_ = route_.front();
    // Stagger nap timing by position so sleepy cats in the same level don't all nap in sync.
    napClock_ = stats_->awakeTime * (0.35f + 0.65f * std::fmod(pos_.x * 0.0131f + pos_.y * 0.0077f, 1.0f));
    if (route_.size() >= 2)
    {
        target_ = 1;
        facing_ = AngleTo(pos_, route_[1]);
    }
}

void Cat::Update(const Level& level, float dt)
{
    repathTimer_ -= dt;

    switch (behavior_)
    {
    case Behavior::Patrol:
        UpdateNap(dt);
        if (!asleep_) UpdatePatrol(dt);
        break;

    case Behavior::Investigate:
        if (FollowPath(dt, stats_->chaseSpeed))
        {
            behavior_ = Behavior::Search;
            timer_ = SearchTime;
            searchBase_ = facing_;
            searchClock_ = 0.0f;
        }
        break;

    case Behavior::Search:
        // Sweep the head left and right around the direction we arrived from.
        searchClock_ += dt;
        facing_ = WrapAngle(searchBase_ + std::sin(searchClock_ * 2.2f) * 1.3f);
        timer_ -= dt;
        if (timer_ <= 0.0f)
        {
            behavior_ = Behavior::Return;
            StartPath(level, route_[target_]);
        }
        break;

    case Behavior::Return:
        if (FollowPath(dt, stats_->patrolSpeed))
        {
            behavior_ = Behavior::Patrol;
            napClock_ = stats_->awakeTime; // stay awake for a while after a scare
            if (route_.size() >= 2)
            {
                AdvanceWaypoint();
                step_ = PatrolStep::Turning;
            }
        }
        break;
    }
}

void Cat::UpdatePatrol(float dt)
{
    // Stationary guard: slowly turn in place.
    if (route_.size() < 2)
    {
        facing_ = WrapAngle(facing_ + 0.8f * dt);
        return;
    }

    const Vector2 target = route_[target_];
    switch (step_)
    {
    case PatrolStep::Waiting:
        timer_ -= dt;
        if (timer_ <= 0.0f) step_ = PatrolStep::Turning;
        break;

    case PatrolStep::Turning:
        if (TurnToward(AngleTo(pos_, target), TurnSpeed * dt)) step_ = PatrolStep::Moving;
        break;

    case PatrolStep::Moving:
    {
        const Vector2 d = Vector2Subtract(target, pos_);
        const float dist = Vector2Length(d);
        const float move = stats_->patrolSpeed * dt;
        if (move >= dist)
        {
            pos_ = target;
            AdvanceWaypoint();
            step_ = PatrolStep::Waiting;
            timer_ = PauseTime;
        }
        else
        {
            pos_ = Vector2Add(pos_, Vector2Scale(d, move / dist));
        }
        break;
    }
    }
}

void Cat::UpdateNap(float dt)
{
    if (stats_->sleepTime <= 0.0f) return;
    napClock_ -= dt;
    if (napClock_ <= 0.0f)
    {
        asleep_ = !asleep_;
        napClock_ = asleep_ ? stats_->sleepTime : stats_->awakeTime;
    }
}

bool Cat::FollowPath(float dt, float speed)
{
    float remaining = speed * dt;
    while (pathIndex_ < path_.size() && remaining > 0.0f)
    {
        const Vector2 d = Vector2Subtract(path_[pathIndex_], pos_);
        const float dist = Vector2Length(d);
        if (dist <= remaining)
        {
            pos_ = path_[pathIndex_++];
            remaining -= dist;
        }
        else
        {
            pos_ = Vector2Add(pos_, Vector2Scale(d, remaining / dist));
            remaining = 0.0f;
        }
    }

    // Look where we're going (turning faster than on patrol).
    if (pathIndex_ < path_.size() && Vector2Distance(pos_, path_[pathIndex_]) > 0.5f)
        TurnToward(AngleTo(pos_, path_[pathIndex_]), TurnSpeed * 2.0f * dt);

    return pathIndex_ >= path_.size();
}

bool Cat::TurnToward(float angle, float maxStep)
{
    const float diff = WrapAngle(angle - facing_);
    if (std::fabs(diff) <= maxStep)
    {
        facing_ = WrapAngle(angle);
        return true;
    }
    facing_ = WrapAngle(facing_ + (diff > 0.0f ? maxStep : -maxStep));
    return false;
}

void Cat::StartPath(const Level& level, Vector2 goal)
{
    path_ = FindPath(level, pos_, goal, Radius + 2.0f);
    pathIndex_ = 0;
    repathTimer_ = RepathInterval;
}

void Cat::AdvanceWaypoint()
{
    const int count = static_cast<int>(route_.size());
    if (target_ + direction_ < 0 || target_ + direction_ >= count) direction_ = -direction_; // reverse at the ends
    target_ += direction_;
}

void Cat::Alert(const Level& level, Vector2 lastSeen)
{
    asleep_ = false;
    lastSeen_ = lastSeen;
    // Already chasing and recently re-planned? Keep the current path for now.
    if (behavior_ == Behavior::Investigate && repathTimer_ > 0.0f) return;
    behavior_ = Behavior::Investigate;
    StartPath(level, lastSeen);
}

bool Cat::CanSee(const Level& level, Vector2 point, float r) const
{
    if (asleep_) return false;

    const Vector2 to = Vector2Subtract(point, pos_);
    const float dist = Vector2Length(to);
    if (dist > stats_->viewRange + r) return false;

    if (dist > r)
    {
        // Widen the cone slightly by the target's size so an edge-on player still counts.
        const float allowance = std::asin(r / dist);
        const float diff = WrapAngle(std::atan2(to.y, to.x) - facing_);
        if (std::fabs(diff) > stats_->viewHalfAngle + allowance) return false;
    }
    return level.HasLineOfSight(pos_, point);
}

void Cat::Draw(const Level& level, float alert, bool seesPlayer) const
{
    const float range = stats_->viewRange;
    const float half = stats_->viewHalfAngle;

    // Vision cone: cast rays across the cone and stop each one at the first wall. No cone while napping.
    if (!asleep_)
    {
        constexpr int rays = 48;
        const Color calm{255, 230, 90, 255};
        const Color suspicious{255, 150, 40, 255};
        const Color angry{255, 60, 40, 255};
        const Color base = IsSuspicious() ? suspicious : calm;
        const Color coneColor = Fade(ColorLerp(base, angry, alert), 0.22f + 0.2f * alert);

        Vector2 prev{};
        for (int i = 0; i <= rays; ++i)
        {
            const float a = facing_ - half + 2.0f * half * static_cast<float>(i) / rays;
            const Vector2 dir = FromAngle(a);
            const Vector2 end = Vector2Add(pos_, Vector2Scale(dir, level.Raycast(pos_, dir, range)));
            if (i > 0) DrawTriangleAnyOrder(pos_, prev, end, coneColor);
            prev = end;
        }
    }

    // Body
    const Vector2 f = FromAngle(facing_);
    const Vector2 side{-f.y, f.x};
    const Vector2 earL = Vector2Add(pos_, Vector2Add(Vector2Scale(f, 8.0f), Vector2Scale(side, 11.0f)));
    const Vector2 earR = Vector2Add(pos_, Vector2Subtract(Vector2Scale(f, 8.0f), Vector2Scale(side, 11.0f)));
    const Vector2 eyeL = Vector2Add(pos_, Vector2Add(Vector2Scale(f, 9.0f), Vector2Scale(side, 5.0f)));
    const Vector2 eyeR = Vector2Add(pos_, Vector2Subtract(Vector2Scale(f, 9.0f), Vector2Scale(side, 5.0f)));
    DrawCircleV(earL, 6.5f, stats_->furDark);
    DrawCircleV(earR, 6.5f, stats_->furDark);
    DrawCircleV(pos_, Radius, stats_->fur);

    if (asleep_)
    {
        // Closed eyes and floating Z's.
        DrawLineEx(Vector2Subtract(eyeL, Vector2Scale(side, 2.5f)), Vector2Add(eyeL, Vector2Scale(side, 2.5f)), 2.0f, stats_->furDark);
        DrawLineEx(Vector2Subtract(eyeR, Vector2Scale(side, 2.5f)), Vector2Add(eyeR, Vector2Scale(side, 2.5f)), 2.0f, stats_->furDark);
        const float bob = std::sin(static_cast<float>(GetTime()) * 3.0f) * 3.0f;
        DrawText("z", static_cast<int>(pos_.x) + 10, static_cast<int>(pos_.y - 26 + bob), 16, SKYBLUE);
        DrawText("Z", static_cast<int>(pos_.x) + 18, static_cast<int>(pos_.y - 40 - bob), 20, SKYBLUE);
        return;
    }

    DrawCircleV(eyeL, 3.0f, stats_->eye);
    DrawCircleV(eyeR, 3.0f, stats_->eye);

    const int tx = static_cast<int>(pos_.x) - 4;
    const int ty = static_cast<int>(pos_.y - Radius) - 30;
    if (seesPlayer)
        DrawText("!", tx, ty, 28, RED);
    else if (IsSuspicious())
        DrawText("?", tx - 2, ty, 28, ORANGE);
}

void Cat::DrawRoute() const
{
    constexpr float spacing = 12.0f;
    for (size_t i = 1; i < route_.size(); ++i)
    {
        const Vector2 a = route_[i - 1];
        const Vector2 b = route_[i];
        const int dots = std::max(1, static_cast<int>(Vector2Distance(a, b) / spacing));
        for (int k = 0; k <= dots; ++k)
            DrawCircleV(Vector2Lerp(a, b, static_cast<float>(k) / dots), 2.2f, Fade(WHITE, 0.55f));
    }
}

void Cat::DrawDebug() const
{
    if (behavior_ == Behavior::Investigate || behavior_ == Behavior::Return)
    {
        Vector2 prev = pos_;
        for (size_t i = pathIndex_; i < path_.size(); ++i)
        {
            DrawLineEx(prev, path_[i], 2.0f, behavior_ == Behavior::Investigate ? ORANGE : SKYBLUE);
            DrawCircleV(path_[i], 3.0f, WHITE);
            prev = path_[i];
        }
    }
    if (IsSuspicious()) DrawCircleLinesV(lastSeen_, 10.0f, RED);
}
