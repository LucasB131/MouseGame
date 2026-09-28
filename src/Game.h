#pragma once

#include <string>
#include <vector>

#include "Cat.h"
#include "CheeseTrail.h"
#include "Level.h"
#include "Player.h"

// "1:05.3" style time string.
std::string FormatTime(float seconds);

// One level being played: the mouse, the cats, cheese, mouse holes, detection, timer and win/lose.
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
    bool AllCheeseFound() const { return cheese_.empty(); }
    bool IsHidden() const { return hiddenIn_ >= 0; }

    static constexpr float CheeseRadius = 10.0f;
    // Seconds a normal cat must see you before you're caught at the edge of its vision.
    // Up close it's 3x faster, so a quick glimpse from far away only makes the cat suspicious.
    static constexpr float DetectTime = 0.9f;
    // Each carried cheese slows the mouse down, to a minimum of 55% speed.
    static constexpr float SlowdownPerCheese = 0.07f;
    static constexpr float MinSpeedFactor = 0.55f;
    static constexpr float TrailSpacing = 22.0f;

    Level level_;
    Player player_;
    CheeseTrail trail_;
    std::vector<Cat> cats_;
    std::vector<bool> catSeesPlayer_;
    std::vector<Vector2> cheese_; // cheese still lying on the map
    int totalCheese_ = 0;
    int carried_ = 0; // picked up, trailing behind the mouse
    int stored_ = 0;  // safely delivered to a mouse hole or the exit

    int hiddenIn_ = -1;   // index of the hole the mouse is hiding in, -1 if out in the open
    int ignoreHole_ = -1; // hole just left; can't re-enter until you step off it
    bool waitForKeyRelease_ = false; // entered a hole while holding a key: release before you can leave

    float detection_ = 0.0f; // 0..1, fills while any cat can see you
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
