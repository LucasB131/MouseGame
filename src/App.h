#pragma once

#include <string>
#include <vector>

#include "Game.h"

// Top-level flow: the homepage (level list) and the level being played. Also saves best times.
class App
{
public:
    void Init();
    bool Update(); // returns false when the player quits
    void Draw() const;

private:
    enum class Screen { Menu, Playing };

    struct LevelEntry
    {
        std::string path;
        std::string name;
        float bestTime = -1.0f; // < 0 = not completed yet
        bool hasKind[CatKindCount] = {};
    };

    void UpdateMenu();
    void UpdatePlaying();
    void DrawMenu() const;
    void StartLevel(int index);
    Rectangle RowRect(int index) const;

    void LoadSave();
    void WriteSave() const;

    std::vector<LevelEntry> levels_;
    std::string savePath_;
    Game game_;
    Screen screen_ = Screen::Menu;
    int current_ = 0;  // level being played
    int selected_ = 0; // highlighted row on the menu
    bool winRecorded_ = false;
    bool quit_ = false;
};
