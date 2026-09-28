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

void DrawStar(Vector2 c, float outer, float rotation, Color color)
{
    const float inner = outer * 0.45f;
    for (int i = 0; i < 5; ++i)
    {
        const float a0 = rotation + i * 2.0f * PI / 5.0f;
        const float aL = a0 - PI / 5.0f;
        const float aR = a0 + PI / 5.0f;
        const Vector2 tip = Vector2Add(c, Vector2Scale(FromAngle(a0), outer));
        DrawTriangleAnyOrder(c, Vector2Add(c, Vector2Scale(FromAngle(aL), inner)), tip, color);
        DrawTriangleAnyOrder(c, tip, Vector2Add(c, Vector2Scale(FromAngle(aR), inner)), color);
    }
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

bool Cat::IsSuspicious() const
{
    return behavior_ == Behavior::Investigate || behavior_ == Behavior::Search || behavior_ == Behavior::WindUp ||
           behavior_ == Behavior::Lunge;
}

void Cat::Update(const Level& level, float dt)
{
    repathTimer_ -= dt;
    lungeCooldown_ -= dt;
    sinceSeen_ += dt;

    switch (behavior_)
    {
    case Behavior::Patrol:
        UpdateNap(dt);
        if (!asleep_) UpdatePatrol(dt);
        break;

    case Behavior::Investigate:
        if (FollowPath(dt, stats_->chaseSpeed)) StartSearch();
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

    case Behavior::WindUp:
        // Keep aiming at the mouse while crouching.
        TurnToward(AngleTo(pos_, lastSeen_), TurnSpeed * 4.0f * dt);
        timer_ -= dt;
        if (timer_ <= 0.0f)
        {
            const Vector2 d = Vector2Subtract(lastSeen_, pos_);
            lungeDir_ = Vector2Length(d) > 1.0f ? Vector2Normalize(d) : FromAngle(facing_);
            facing_ = std::atan2(lungeDir_.y, lungeDir_.x);
            lungeTraveled_ = 0.0f;
            behavior_ = Behavior::Lunge;
        }
        break;

    case Behavior::Lunge:
        UpdateLunge(level, dt);
        break;

    case Behavior::Stunned:
        timer_ -= dt;
        if (timer_ <= 0.0f) StartSearch(); // lost track of the mouse while dazed
        break;
    }
}

void Cat::UpdateLunge(const Level& level, float dt)
{
    // Move in small steps so a fast lunge can't skip through a wall.
    float remaining = LungeSpeed * dt;
    while (remaining > 0.0f)
    {
        const float step = std::min(4.0f, remaining);
        const Vector2 next = Vector2Add(pos_, Vector2Scale(lungeDir_, step));
        if (level.CircleOverlapsWall(next, Radius))
        {
            // Smacked into a wall: dazed.
            behavior_ = Behavior::Stunned;
            timer_ = StunTime;
            path_.clear();
            return;
        }
        pos_ = next;
        remaining -= step;
        lungeTraveled_ += step;
        if (lungeTraveled_ >= LungeDistance)
        {
            // Missed. Still have eyes on the mouse? Keep chasing. Otherwise lose interest and look around.
            lungeCooldown_ = LungeCooldown;
            if (sinceSeen_ < 0.3f)
            {
                behavior_ = Behavior::Investigate;
                StartPath(level, lastSeen_);
            }
            else
            {
                StartSearch();
            }
            return;
        }
    }
}

void Cat::StartSearch()
{
    behavior_ = Behavior::Search;
    timer_ = SearchTime;
    searchBase_ = facing_;
    searchClock_ = 0.0f;
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
    sinceSeen_ = 0.0f;

    // Mid-pounce or dazed: just remember where the mouse is.
    if (behavior_ == Behavior::WindUp || behavior_ == Behavior::Lunge || behavior_ == Behavior::Stunned) return;

    // Close enough to pounce?
    if (lungeCooldown_ <= 0.0f && Vector2Distance(pos_, lastSeen) <= LungeRange)
    {
        behavior_ = Behavior::WindUp;
        timer_ = WindUpTime;
        return;
    }

    // Otherwise chase. Already chasing and recently re-planned? Keep the current path for now.
    if (behavior_ == Behavior::Investigate && repathTimer_ > 0.0f) return;
    behavior_ = Behavior::Investigate;
    StartPath(level, lastSeen);
}

bool Cat::CanSee(const Level& level, Vector2 point, float r) const
{
    if (asleep_ || behavior_ == Behavior::Stunned) return false;

    const Vector2 to = Vector2Subtract(point, pos_);
    const float dist = Vector2Length(to);
    if (dist > stats_->viewRange + r) return false;

    if (dist > r && stats_->viewHalfAngle < PI)
    {
        // Widen the cone slightly by the target's size so an edge-on mouse still counts.
        const float allowance = std::asin(r / dist);
        const float diff = WrapAngle(std::atan2(to.y, to.x) - facing_);
        if (std::fabs(diff) > stats_->viewHalfAngle + allowance) return false;
    }
    return level.HasLineOfSight(pos_, point);
}

void Cat::Draw(const Level& level, bool seesPlayer) const
{
    const float range = stats_->viewRange;
    const float half = stats_->viewHalfAngle;
    const float t = static_cast<float>(GetTime());

    // Vision area: cast rays across the cone (or all the way around) and stop each at the first wall.
    if (!asleep_ && behavior_ != Behavior::Stunned)
    {
        const int rays = half >= PI ? 90 : 48;
        const Color calm{255, 230, 90, 255};
        const Color suspicious{255, 150, 40, 255};
        const Color angry{255, 60, 40, 255};
        const Color base = seesPlayer ? angry : (IsSuspicious() ? suspicious : calm);
        const Color coneColor = Fade(base, seesPlayer ? 0.36f : 0.22f);

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

    const Vector2 f = FromAngle(facing_);
    const Vector2 side{-f.y, f.x};

    // Wind-up: crouch back and tremble. Lunge: motion blur behind the body.
    Vector2 body = pos_;
    if (behavior_ == Behavior::WindUp)
        body = Vector2Add(Vector2Subtract(pos_, Vector2Scale(f, 5.0f)), Vector2Scale(side, std::sin(t * 70.0f) * 1.5f));
    if (behavior_ == Behavior::Lunge)
    {
        for (int i = 3; i >= 1; --i)
            DrawCircleV(Vector2Subtract(pos_, Vector2Scale(lungeDir_, 12.0f * i)), Radius - 2.0f * i, Fade(stats_->fur, 0.12f * (4 - i)));
    }

    const Vector2 earL = Vector2Add(body, Vector2Add(Vector2Scale(f, 8.0f), Vector2Scale(side, 11.0f)));
    const Vector2 earR = Vector2Add(body, Vector2Subtract(Vector2Scale(f, 8.0f), Vector2Scale(side, 11.0f)));
    const Vector2 eyeL = Vector2Add(body, Vector2Add(Vector2Scale(f, 9.0f), Vector2Scale(side, 5.0f)));
    const Vector2 eyeR = Vector2Add(body, Vector2Subtract(Vector2Scale(f, 9.0f), Vector2Scale(side, 5.0f)));
    DrawCircleV(earL, 6.5f, stats_->furDark);
    DrawCircleV(earR, 6.5f, stats_->furDark);
    DrawCircleV(body, Radius, stats_->fur);

    if (asleep_)
    {
        // Closed eyes and floating Z's.
        DrawLineEx(Vector2Subtract(eyeL, Vector2Scale(side, 2.5f)), Vector2Add(eyeL, Vector2Scale(side, 2.5f)), 2.0f, stats_->furDark);
        DrawLineEx(Vector2Subtract(eyeR, Vector2Scale(side, 2.5f)), Vector2Add(eyeR, Vector2Scale(side, 2.5f)), 2.0f, stats_->furDark);
        const float bob = std::sin(t * 3.0f) * 3.0f;
        DrawText("z", static_cast<int>(pos_.x) + 10, static_cast<int>(pos_.y - 26 + bob), 16, SKYBLUE);
        DrawText("Z", static_cast<int>(pos_.x) + 18, static_cast<int>(pos_.y - 40 - bob), 20, SKYBLUE);
        return;
    }

    if (behavior_ == Behavior::Stunned)
    {
        // X eyes and stars circling above the head.
        for (const Vector2& e : {eyeL, eyeR})
        {
            DrawLineEx({e.x - 3, e.y - 3}, {e.x + 3, e.y + 3}, 2.0f, stats_->furDark);
            DrawLineEx({e.x - 3, e.y + 3}, {e.x + 3, e.y - 3}, 2.0f, stats_->furDark);
        }
        for (int i = 0; i < 3; ++i)
        {
            const float a = t * 5.0f + i * 2.0f * PI / 3.0f;
            const Vector2 star{pos_.x + std::cos(a) * 16.0f, pos_.y - Radius - 10.0f + std::sin(a) * 5.0f};
            DrawStar(star, 6.0f, t * 4.0f, YELLOW);
        }
        return;
    }

    DrawCircleV(eyeL, 3.0f, stats_->eye);
    DrawCircleV(eyeR, 3.0f, stats_->eye);

    const int tx = static_cast<int>(pos_.x) - 4;
    const int ty = static_cast<int>(pos_.y - Radius) - 30;
    if (behavior_ == Behavior::WindUp || behavior_ == Behavior::Lunge)
        DrawText("!!", tx - 6, ty - 2, 30, RED);
    else if (seesPlayer)
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
    if (behavior_ == Behavior::WindUp || behavior_ == Behavior::Lunge)
        DrawCircleLinesV(pos_, LungeRange, Fade(RED, 0.5f));
    if (IsSuspicious()) DrawCircleLinesV(lastSeen_, 10.0f, RED);
}
