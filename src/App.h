#pragma once

#include <string>
#include <vector>

#include "Game.h"

// Top-level flow: intro -> world select -> level select -> playing. Also saves best times.
class App
{
public:
    void Init();
    bool Update(); // returns false when the player quits
    void Draw() const;

private:
    enum class Screen { Intro, Worlds, Levels, Playing };

    static constexpr int WorldCount = 5;
    static constexpr int SlotsPerWorld = 25;
    static constexpr int GridColumns = 5;
    static constexpr float IntroLength = 6.0f; // seconds

    struct LevelEntry
    {
        std::string path;
        std::string name;
        float bestTime = -1.0f; // < 0 = not completed yet
        bool hasKind[CatKindCount] = {};
    };

    void UpdateIntro();
    void UpdateWorlds();
    void UpdateLevels();
    void UpdatePlaying();

    void DrawIntro() const;
    void DrawWorlds() const;
    void DrawLevels() const;
    void DrawBackground() const;

    void StartLevel(int index);
    int ClearedCount() const;
    bool IsUnlocked(int index) const; // level n+1 opens once level n is beaten
    Rectangle WorldCardRect(int index) const;
    Rectangle LevelSlotRect(int index) const;

    void LoadSave();
    void WriteSave() const;

    std::vector<LevelEntry> levels_; // World 1's playable levels
    std::string savePath_;
    Game game_;
    Screen screen_ = Screen::Intro;
    float introTime_ = 0.0f;
    int selectedWorld_ = 0;
    int selectedSlot_ = 0;
    int current_ = 0; // level being played
    bool winRecorded_ = false;
    bool quit_ = false;
};
