#pragma once

#include <string>
#include <vector>

#include "Game.h"
#include "Profile.h"

// Top-level flow: intro -> world select -> level select -> playing, plus the shop (from world select).
// Also saves best times and the player's coins and skins.
class App
{
public:
    void Init();
    bool Update(); // returns false when the player quits
    void Draw() const;

private:
    enum class Screen { Intro, Worlds, Levels, Playing, Shop };

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
        bool isBoss = false;
    };

    static constexpr int BossSlot = SlotsPerWorld; // index of the boss in levels_, after the 25 regular levels

    void UpdateIntro();
    void UpdateWorlds();
    void UpdateLevels();
    void UpdatePlaying();
    void UpdateShop();
    void UpdateMusic(); // picks the track for the current screen (menu, house, Egypt or boss)

    void DrawIntro() const;
    void DrawWorlds() const;
    void DrawLevels() const;
    void DrawShop() const;
    void DrawBackground() const;

    void StartLevel(int index);
    int ClearedCount() const;         // regular levels only
    bool IsUnlocked(int index) const; // level n+1 opens once level n is beaten; the boss once all 25 are
    bool HasBoss() const { return static_cast<int>(levels_.size()) > BossSlot; }
    bool BossBeaten() const { return HasBoss() && levels_[BossSlot].bestTime >= 0.0f; }
    bool WorldUnlocked(int world) const { return world == 0 || (world == 1 && summary_[0].bossBeaten); }
    void LoadWorld(int world);  // fills levels_ + savePath_ for that world
    void RefreshSummary();      // updates summary_ for the active world
    Rectangle BossSlotRect() const;
    void DrawBossSlot() const;
    Rectangle WorldCardRect(int index) const;
    Rectangle LevelSlotRect(int index) const;
    Rectangle ShopButtonRect() const;
    Rectangle ShopCardRect(int index) const;
    Rectangle ShopTabRect(int index) const;
    void ActivateShopCard(int index);
    SkinCategory ShopCategory() const { return static_cast<SkinCategory>(shopTab_); }

    void LoadSave();
    void WriteSave() const;

    // What the world-select cards show, kept for every world so they don't depend on which one is loaded.
    struct WorldSummary
    {
        int cleared = 0;
        int playable = 0;
        bool bossBeaten = false;
    };

    std::vector<LevelEntry> levels_; // the playable levels of the active world
    WorldSummary summary_[WorldCount];
    int activeWorld_ = 0;
    std::string savePath_;
    Profile profile_;
    int shopSel_ = 0; // selected card within the current tab
    int shopTab_ = 0; // SkinCategory shown in the shop
    std::string shopToast_;
    float shopToastTimer_ = 0.0f;
    bool shopToastGood_ = true;
    Game game_;
    Screen screen_ = Screen::Intro;
    float introTime_ = 0.0f;
    bool introSounded_ = false;
    int selectedWorld_ = 0;
    int selectedSlot_ = 0;
    int lastGridSlot_ = 4;     // where Left returns to from the boss slot
    float toastTimer_ = 0.0f;  // "World 2 is coming soon" message
    int current_ = 0; // level being played
    bool winRecorded_ = false;
    bool quit_ = false;
};
