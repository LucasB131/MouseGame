#pragma once

#include <vector>

#include "CatTypes.h"
#include "raylib.h"

class Level;

// A guard with a simple behavior state machine:
//   Patrol      -> walk the route forward then back, pausing and turning at each waypoint
//   Investigate -> spotted the player: run (A* path) to where they were last seen
//   Search      -> look around at that spot for a few seconds
//   Return      -> walk (A* path) back to the patrol route and resume
// Speed, vision and napping come from the cat's kind (see CatTypes.h).
class Cat
{
public:
    static constexpr float Radius = 16.0f;

    Cat(CatKind kind, std::vector<Vector2> route);

    void Update(const Level& level, float dt);

    // True if the point (a circle of radius r) is inside the vision cone and not hidden behind a wall.
    bool CanSee(const Level& level, Vector2 point, float r) const;

    // Call every frame the cat can see the player: remembers the position and goes to investigate.
    void Alert(const Level& level, Vector2 lastSeen);

    bool IsSuspicious() const { return behavior_ == Behavior::Investigate || behavior_ == Behavior::Search; }
    bool IsAsleep() const { return asleep_; }
    Vector2 Position() const { return pos_; }
    float ViewRange() const { return stats_->viewRange; }
    float DetectMultiplier() const { return stats_->detectMultiplier; }

    // alert: 0 = calm, 1 = about to catch the player. Tints the vision cone.
    void Draw(const Level& level, float alert, bool seesPlayer) const;
    void DrawRoute() const; // dotted white line along the patrol route
    void DrawDebug() const; // current A* path, last known player position

private:
    enum class Behavior { Patrol, Investigate, Search, Return };
    enum class PatrolStep { Waiting, Turning, Moving };

    void UpdatePatrol(float dt);
    void UpdateNap(float dt);
    bool FollowPath(float dt, float speed); // returns true once the end of the path is reached
    bool TurnToward(float angle, float maxStep); // returns true once facing the angle
    void StartPath(const Level& level, Vector2 goal);
    void AdvanceWaypoint();

    const CatStats* stats_;

    // Napping (only for kinds with sleepTime > 0)
    bool asleep_ = false;
    float napClock_ = 0.0f; // seconds until switching between awake and asleep

    // Patrol route
    std::vector<Vector2> route_;
    int target_ = 0;    // index of the waypoint we're heading to
    int direction_ = 1; // +1 walking forward through the route, -1 walking back

    Vector2 pos_{};
    float facing_ = 0.0f; // radians, 0 = right, positive = clockwise on screen

    Behavior behavior_ = Behavior::Patrol;
    PatrolStep step_ = PatrolStep::Moving;
    float timer_ = 0.0f;

    // Pathfinding (Investigate / Return)
    std::vector<Vector2> path_;
    size_t pathIndex_ = 0;
    float repathTimer_ = 0.0f;
    Vector2 lastSeen_{};

    // Search
    float searchBase_ = 0.0f;
    float searchClock_ = 0.0f;

    static constexpr float TurnSpeed = 3.5f;        // radians per second
    static constexpr float PauseTime = 0.8f;        // seconds at each patrol waypoint
    static constexpr float SearchTime = 2.5f;       // seconds spent looking around
    static constexpr float RepathInterval = 0.2f;   // min seconds between path recalculations
};
