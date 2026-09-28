#pragma once

#include <string>
#include <vector>

#include "Cat.h"
#include "CheeseTrail.h"
#include "Level.h"
#include "Player.h"
#include "RoomRenderer.h"

// "1:05.3" style time string.
std::string FormatTime(float seconds);

// One level being played: the mouse, the cats, cheese, mouse holes, timer and win/lose.
// Cats that see you chase you; you're only caught if one touches you (usually with a lunge).
class Game
{
public:
    // bestTime < 0 means the level hasn't been completed yet.
    bool Init(const std::string& levelPath, float bestTime, bool hasNextLevel);
    void Update(float dt);
    void Draw() const;

    bool IsWon() const { return state_ == State::Won; }
    float BestTime() const { return bestTime_; }

private:
    enum class State { Playing, Caught, Won };

    void Reset();
    void UpdatePlayer(float dt);
    void UpdateCats(float dt);
    void EnterHole(int index);
    void StoreCheese(Vector2 where);
    bool IsFlashingRed() const;
    bool AllCheeseFound() const { return cheese_.empty(); }
    bool IsHidden() const { return hiddenIn_ >= 0; }

    static constexpr float CheeseRadius = 10.0f;
    // Each carried cheese slows the mouse down, to a minimum of 55% speed.
    static constexpr float SlowdownPerCheese = 0.07f;
    static constexpr float MinSpeedFactor = 0.55f;
    static constexpr float TrailSpacing = 22.0f;
    // Red pepper: temporary speed boost (stacks with the cheese slowdown).
    static constexpr float PepperBoost = 1.5f;
    static constexpr float PepperDuration = 5.0f; // seconds
    static constexpr float PepperRadius = 12.0f;

    Level level_;
    RoomRenderer renderer_;
    Player player_;
    CheeseTrail trail_;
    std::vector<Cat> cats_;
    std::vector<bool> catSeesPlayer_;
    std::vector<Vector2> cheese_; // cheese still lying on the map
    std::vector<Vector2> peppers_; // peppers still lying on the map
    float boostTimer_ = 0.0f;      // seconds of pepper boost left
    float flashPhase_ = 0.0f;      // drives the red flashing while boosted
    int totalCheese_ = 0;
    int carried_ = 0; // picked up, trailing behind the mouse
    int stored_ = 0;  // safely delivered to a mouse hole or the exit

    int hiddenIn_ = -1;   // index of the hole the mouse is hiding in, -1 if out in the open
    int ignoreHole_ = -1; // hole just left; can't re-enter until you step off it
    bool waitForKeyRelease_ = false; // entered a hole while holding a key: release before you can leave

    float elapsed_ = 0.0f;   // seconds since the level started
    float bestTime_ = -1.0f;
    bool newBest_ = false;
    bool hasNextLevel_ = false;
    State state_ = State::Playing;
    bool debug_ = false; // F1: show A* paths

    // "+3 stored" popup
    float popupTimer_ = 0.0f;
    Vector2 popupPos_{};
    int popupCount_ = 0;
};
