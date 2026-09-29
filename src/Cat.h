#pragma once

#include <vector>

#include "CatTypes.h"
#include "raylib.h"

class Level;

// A guard with a behavior state machine:
//   Patrol      -> walk the route forward then back, pausing and turning at each waypoint
//   Investigate -> spotted the mouse: chase it along an A* path to where it was last seen
//   Search      -> look around at that spot for a few seconds
//   Return      -> walk (A* path) back to the patrol route and resume
//   WindUp      -> mouse is close: crouch for a moment, aiming
//   Lunge       -> dash in a straight line. Touching the mouse catches it; hitting a wall stuns the cat
//   Stunned     -> dazed (spinning stars): can't see or move, then gives up and searches
//   KnockedOut  -> bosses only: after crashing BossHitsToWin times, out cold for good
// Speed, vision and napping come from the cat's kind (see CatTypes.h).
class Cat
{
public:
    static constexpr int BossHitsToWin = 3;

    Cat(CatKind kind, std::vector<Vector2> route);

    void Update(const Level& level, float dt);

    // True if the point (a circle of radius r) is within the cat's senses and not hidden behind a wall.
    bool CanSee(const Level& level, Vector2 point, float r) const;

    // Call every frame the cat can see the mouse: chase it, or lunge if it's close enough.
    void Alert(const Level& level, Vector2 lastSeen);

    bool IsSuspicious() const;
    bool IsLunging() const { return behavior_ == Behavior::Lunge; }
    bool IsStunned() const { return behavior_ == Behavior::Stunned; }
    bool IsKnockedOut() const { return behavior_ == Behavior::KnockedOut; }
    bool IsHarmless() const { return IsStunned() || IsKnockedOut(); }
    bool IsBoss() const { return stats_->boss; }
    int BossHits() const { return hits_; }
    // Seconds before the pounce that the boss stops tracking the mouse; the red lane brightens at this moment.
    float AimLockTime() const { return BossAimLock - 0.05f * static_cast<float>(hits_); }
    float BodyRadius() const { return stats_->radius; }
    const char* Name() const { return stats_->name; }
    CatKind Kind() const { return kind_; }
    bool IsAsleep() const { return asleep_; }
    Vector2 Position() const { return pos_; }

    void Draw(const Level& level, bool seesPlayer, int variant = 0) const; // variant: shop costume
    void DrawRoute() const; // dotted white line along the patrol route
    void DrawDebug() const; // current A* path, last known mouse position

private:
    enum class Behavior { Patrol, Investigate, Search, Return, WindUp, Lunge, Stunned, KnockedOut };
    enum class PatrolStep { Waiting, Turning, Moving };

    void UpdatePatrol(float dt);
    void UpdateNap(float dt);
    void UpdateLunge(const Level& level, float dt);
    bool FollowPath(float dt, float speed); // returns true once the end of the path is reached
    bool TurnToward(float angle, float maxStep); // returns true once facing the angle
    void StartPath(const Level& level, Vector2 goal);
    void StartSearch();
    void AdvanceWaypoint();

    CatKind kind_;
    const CatStats* stats_;

    // Napping (only for kinds with sleepTime > 0)
    bool asleep_ = false;
    float napClock_ = 0.0f; // seconds until switching between awake and asleep

    // Patrol route
    std::vector<Vector2> route_;
    int target_ = 0;    // index of the waypoint we're heading to
    int direction_ = 1; // +1 walking forward through the route, -1 walking back

    Vector2 pos_{};
    Vector2 prevPos_{};
    float walkPhase_ = 0.0f; // paw animation
    bool moving_ = false;
    float facing_ = 0.0f; // radians, 0 = right, positive = clockwise on screen

    Behavior behavior_ = Behavior::Patrol;
    PatrolStep step_ = PatrolStep::Moving;
    float timer_ = 0.0f;

    // Chasing
    std::vector<Vector2> path_;
    size_t pathIndex_ = 0;
    float repathTimer_ = 0.0f;
    Vector2 lastSeen_{};
    float sinceSeen_ = 99.0f; // seconds since the mouse was last seen

    // Search
    float searchBase_ = 0.0f;
    float searchClock_ = 0.0f;

    // Lunge
    Vector2 lungeDir_{1.0f, 0.0f};
    Vector2 lungeTarget_{};  // where the pounce is aimed (bosses lock their aim just before pouncing)
    int hits_ = 0;           // bosses: how many times it has crashed
    float lungeTraveled_ = 0.0f;
    float lungeCooldown_ = 0.0f;

    static constexpr float TurnSpeed = 3.5f;       // radians per second
    static constexpr float PauseTime = 0.8f;       // seconds at each patrol waypoint
    static constexpr float SearchTime = 2.5f;      // seconds spent looking around
    static constexpr float RepathInterval = 0.2f;  // min seconds between path recalculations
    static constexpr float LungeCooldown = 1.2f;   // seconds before it can lunge again after a miss
    static constexpr float BossAimLock = 0.5f;     // bosses stop tracking the mouse this long before pouncing (less each hit)
};
