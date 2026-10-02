#pragma once

#include <string>
#include <vector>

#include "Audio.h"
#include "Cat.h"
#include "CheeseTrail.h"
#include "Level.h"
#include "Player.h"
#include "RoomRenderer.h"
#include "Skins.h"

// "1:05.3" style time string.
std::string FormatTime(float seconds);

// One level being played: the mouse, the cats, cheese, mouse holes, timer and win/lose.
// Cats that see you chase you; you're only caught if one touches you (usually with a lunge).
class Game
{
public:
    // bestTime < 0 means the level hasn't been completed yet.
    // nextIsBoss: the level after this one is the world's boss (changes the "next level" prompt).
    bool Init(const std::string& levelPath, float bestTime, bool hasNextLevel, bool nextIsBoss = false);
    void Update(float dt);
    void Draw() const;

    bool IsWon() const { return state_ == State::Won; }
    void SetMouseSkin(MouseSkin skin) { skin_ = skin; }
    // Which shop costume every cat of a type wears (0 = default).
    void SetCatVariant(CatKind kind, int variant) { catVariant_[static_cast<int>(kind)] = variant; }
    // Coins for this win: the cheese you got home, plus a bonus the first time the level is cleared.
    int CoinsEarned() const { return storedCoins_ + (firstClear_ ? (bossIndex_ >= 0 ? BossClearBonus : ClearBonus) : 0); }
    float BestTime() const { return bestTime_; }

private:
    enum class State { Playing, Caught, Won };

    void Reset();
    void UpdatePlayer(float dt);
    void UpdateCats(float dt);
    void EnterHole(int index, bool teleport = false);
    void StoreCheese(Vector2 where, bool silent = false);
    void CatSounds(size_t index, float dt); // plays a sound whenever a cat changes what it is doing
    bool IsFlashingRed() const;
    // The exit opens once every piece of cheese is picked up (and, on a boss level, the boss is out cold
    // and its Golden Cheese has been grabbed).
    bool AllCheeseFound() const { return cheese_.empty() && (bossIndex_ < 0 || goldenDropped_); }
    void UpdateBoss();
    void DrawBossHud() const;
    bool IsHidden() const { return hiddenIn_ >= 0; }

    static constexpr int ClearBonus = 10;      // coins the first time a level is beaten
    static constexpr int BossClearBonus = 50;  // ...and for beating the boss
    static constexpr float CheeseRadius = 10.0f;
    // Each unit of carried cheese weight slows the mouse down, to a minimum of 55% speed.
    static constexpr float SlowdownPerCheese = 0.07f;
    static constexpr float MinSpeedFactor = 0.55f;
    static constexpr float TrailSpacing = 22.0f;
    static constexpr float StepSpacing = 26.0f; // pixels walked per footstep sound
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
    struct CatSoundState
    {
        bool chasing = false, windingUp = false, lunging = false, stunned = false, knockedOut = false;
        float alertCooldown = 0.0f;
    };
    std::vector<CatSoundState> catSound_; // what each cat was doing last frame, to spot changes
    float stepDistance_ = 0.0f;           // distance walked since the last footstep sound
    bool stepFlip_ = false;
    bool exitWasOpen_ = false;
    struct CheesePiece
    {
        Vector2 pos;
        CheeseKind kind;
    };
    std::vector<CheesePiece> cheese_;     // cheese still lying on the map
    std::vector<CheeseKind> carriedKinds_; // trailing behind the mouse, in pickup order
    std::vector<Vector2> peppers_; // peppers still lying on the map
    float boostTimer_ = 0.0f;      // seconds of pepper boost left
    float flashPhase_ = 0.0f;      // drives the red flashing while boosted
    int totalCheese_ = 0;
    int carried_ = 0;      // pieces trailing behind the mouse
    int carriedWeight_ = 0; // Gouda wheels count double
    int stored_ = 0;  // safely delivered to a mouse hole or the exit
    int storedCoins_ = 0;      // shop coins those pieces are worth (Gouda 2, Golden Cheese 25)
    bool firstClear_ = false;  // this win is the level's first clear
    MouseSkin skin_ = MouseSkin::Classic;
    int catVariant_[CatKindCount] = {};

    int hiddenIn_ = -1;   // index of the hole the mouse is hiding in, -1 if out in the open
    int ignoreHole_ = -1; // hole just left; can't re-enter until you step off it
    bool waitForKeyRelease_ = false; // entered a hole while holding a key: release before you can leave

    float elapsed_ = 0.0f;   // seconds since the level started
    float bestTime_ = -1.0f;
    bool newBest_ = false;
    bool hasNextLevel_ = false;
    bool nextIsBoss_ = false;
    int bossIndex_ = -1;         // index into cats_ of this level's boss, or -1
    bool goldenDropped_ = false; // the knocked-out boss has dropped the Golden Cheese
    int bossHitsShown_ = 0;      // to pop a "HIT!" message when the boss crashes
    float bossFlash_ = 0.0f;
    State state_ = State::Playing;
    bool debug_ = false; // F1: show A* paths

    // "+3 stored" popup
    float popupTimer_ = 0.0f;
    Vector2 popupPos_{};
    int popupCount_ = 0;
};
