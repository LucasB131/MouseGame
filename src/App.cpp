#include "App.h"

#include <algorithm>
#include <cmath>
#include <fstream>

#include "Sprites.h"
#include "raylib.h"

namespace
{
const char* const kWorldNames[] = {"The House", "The Garden", "The Barn", "The Factory", "The City"};
const char* const kCredit = "made by Lucas";

// World select layout
constexpr int CardWidth = 200;
constexpr int CardHeight = 320;
constexpr int CardGap = 24;
constexpr int CardTop = 190;

// Level select layout
constexpr int SlotWidth = 130;
constexpr int SlotHeight = 78;
constexpr int SlotGap = 16;
constexpr int GridTop = 120;
constexpr int BossWidth = 190;
constexpr int BossGap = 24;

const Color kPanel{70, 58, 50, 255};
const Color kPanelHover{96, 76, 56, 255};
const Color kLocked{22, 20, 19, 255};
const Color kLockedEdge{48, 44, 41, 255};
const Color kCleared{60, 170, 90, 255};

// Hovering only changes the selection when the mouse actually moves, so a resting cursor
// doesn't fight the keyboard.
bool MouseMoved()
{
    const Vector2 d = GetMouseDelta();
    return d.x != 0.0f || d.y != 0.0f;
}

void DrawCentered(const char* text, int cx, int y, int size, Color color)
{
    DrawText(text, cx - MeasureText(text, size) / 2, y, size, color);
}

// A gold coin (the shop currency).
void DrawCoin(Vector2 c, float r)
{
    DrawCircleV(c, r, Color{176, 122, 14, 255});
    DrawCircleV(c, r * 0.85f, GOLD);
    DrawCircleV(c, r * 0.6f, Color{240, 190, 30, 255});
    DrawCircleLinesV(c, r * 0.6f, Color{200, 145, 20, 255});
    DrawCircleV({c.x - r * 0.3f, c.y - r * 0.3f}, r * 0.16f, Fade(WHITE, 0.7f));
}

void DrawOpenPadlock(Vector2 c, float s, Color color)
{
    DrawRing({c.x + 8 * s, c.y - 10 * s}, 6 * s, 9 * s, 180.0f, 360.0f, 24, color); // shackle swung open
    DrawRectangleRounded({c.x - 12 * s, c.y - 4 * s, 24 * s, 18 * s}, 0.3f, 6, color);
}

void DrawPadlock(Vector2 c, float s, Color color)
{
    DrawRing({c.x, c.y - 4 * s}, 6 * s, 9 * s, 180.0f, 360.0f, 24, color);             // shackle
    DrawRectangleRounded({c.x - 12 * s, c.y - 4 * s, 24 * s, 18 * s}, 0.3f, 6, color); // body
    DrawCircleV({c.x, c.y + 4 * s}, 2.5f * s, kLocked);                                // keyhole
}

} // namespace

void App::Init()
{
    const std::string dir = GetApplicationDirectory();
    savePath_ = dir + "save_world1.txt"; // v2 levels (the house rooms); older saves are ignored
    profile_.Load(dir + "profile.txt");    // coins and skins

    // Find level1.txt, level2.txt, ... until one is missing, so new levels are picked up automatically.
    for (int i = 1; i <= SlotsPerWorld; ++i)
    {
        const std::string path = dir + TextFormat("assets/levels/level%d.txt", i);
        if (!FileExists(path.c_str())) break;

        Level level;
        if (!level.LoadFromFile(path)) break;

        LevelEntry entry;
        entry.path = path;
        entry.name = level.Name().empty() ? TextFormat("Level %d", i) : level.Name();
        for (const CatSpawn& cat : level.Cats()) entry.hasKind[static_cast<int>(cat.kind)] = true;
        levels_.push_back(entry);
    }

    // The secret boss level comes after all 25 regular ones.
    const std::string bossPath = dir + "assets/levels/boss1.txt";
    Level boss;
    if (static_cast<int>(levels_.size()) == SlotsPerWorld && FileExists(bossPath.c_str()) && boss.LoadFromFile(bossPath))
    {
        LevelEntry entry;
        entry.path = bossPath;
        entry.name = boss.Name().empty() ? "Boss" : boss.Name();
        entry.isBoss = true;
        for (const CatSpawn& cat : boss.Cats()) entry.hasKind[static_cast<int>(cat.kind)] = true;
        levels_.push_back(entry);
    }
    LoadSave();
}

bool App::Update()
{
    switch (screen_)
    {
    case Screen::Intro:   UpdateIntro(); break;
    case Screen::Worlds:  UpdateWorlds(); break;
    case Screen::Levels:  UpdateLevels(); break;
    case Screen::Playing: UpdatePlaying(); break;
    case Screen::Shop:    UpdateShop(); break;
    }
    return !quit_;
}

// ---------------------------------------------------------------- Intro

void App::UpdateIntro()
{
    introTime_ += GetFrameTime();
    const bool skip = IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (skip || introTime_ >= IntroLength) screen_ = Screen::Worlds;
}

void App::DrawIntro() const
{
    const int w = GetScreenWidth();
    const int h = GetScreenHeight();
    const Color bg{14, 12, 11, 255};
    ClearBackground(bg);

    // Fade in over 1.2s, hold, fade out over the last 1.2s.
    const float t = introTime_;
    const float alpha = std::clamp(std::min(t / 1.2f, (IntroLength - t) / 1.2f), 0.0f, 1.0f);

    // The mouse creeps toward a wheel of Gouda while a sleepy cat dozes nearby.
    const float creep = std::min(t / IntroLength, 1.0f) * 70.0f;
    CatLook cat;
    cat.pos = {w / 2.0f + 330, h / 2.0f - 150};
    cat.facing = {-1, 0};
    cat.kind = CatKind::Sleepy;
    cat.pose = CatPose::Asleep;
    cat.scale = 2.2f;
    DrawCatSprite(cat);
    MouseLook mouse;
    mouse.pos = {w / 2.0f - 170 + creep, h / 2.0f - 70};
    mouse.facing = {1, 0};
    mouse.moving = true;
    mouse.walkPhase = t * 9.0f;
    mouse.scale = 3.4f;
    DrawMouseSprite(mouse);
    DrawCheeseSprite(CheeseKind::Gouda, {w / 2.0f + 150, h / 2.0f - 70}, PI, 3.2f);
    DrawCheeseSprite(CheeseKind::Swiss, {w / 2.0f + 215, h / 2.0f - 40}, -2.4f, 2.0f);

    DrawCentered("MouseGame", w / 2, h / 2 + 30, 90, GOLD);
    DrawCentered(kCredit, w / 2, h / 2 + 130, 26, LIGHTGRAY);

    // Fade everything in and out with one overlay.
    DrawRectangle(0, 0, w, h, Fade(bg, 1.0f - alpha));
    DrawCentered("Press Space to skip", w / 2, h - 40, 18, Fade(GRAY, 0.6f));
}

// ---------------------------------------------------------------- World select

Rectangle App::WorldCardRect(int index) const
{
    const int total = WorldCount * CardWidth + (WorldCount - 1) * CardGap;
    const int x = GetScreenWidth() / 2 - total / 2 + index * (CardWidth + CardGap);
    return {static_cast<float>(x), static_cast<float>(CardTop), static_cast<float>(CardWidth), static_cast<float>(CardHeight)};
}

void App::UpdateWorlds()
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        quit_ = true;
        return;
    }
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) selectedWorld_ = std::min(selectedWorld_ + 1, WorldCount - 1);
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) selectedWorld_ = std::max(selectedWorld_ - 1, 0);

    bool clicked = false;
    for (int i = 0; i < WorldCount; ++i)
    {
        if (!CheckCollisionPointRec(GetMousePosition(), WorldCardRect(i))) continue;
        clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        if (clicked || MouseMoved()) selectedWorld_ = i;
    }

    toastTimer_ = std::max(0.0f, toastTimer_ - GetFrameTime());
    if (IsKeyPressed(KEY_S) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), ShopButtonRect())))
    {
        screen_ = Screen::Shop;
        shopSel_ = static_cast<int>(profile_.Equipped());
        shopToastTimer_ = 0.0f;
        return;
    }
    const bool activate = clicked || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
    if (activate && selectedWorld_ == 0)
    {
        screen_ = Screen::Levels;
        selectedSlot_ = 0;
    }
    else if (activate && WorldUnlocked(selectedWorld_))
    {
        toastTimer_ = 2.5f; // unlocked, but not built yet
    }
}

void App::DrawWorlds() const
{
    const int w = GetScreenWidth();
    const int h = GetScreenHeight();
    DrawBackground();

    DrawCentered("MouseGame", w / 2 + 4, 44, 80, {0, 0, 0, 120});
    DrawCentered("MouseGame", w / 2, 40, 80, GOLD);
    DrawCentered("Choose a world", w / 2, 132, 26, LIGHTGRAY);

    // Shop button with the coin balance, top right.
    {
        const Rectangle b = ShopButtonRect();
        const bool hot = CheckCollisionPointRec(GetMousePosition(), b);
        DrawRectangleRounded(b, 0.35f, 8, hot ? kPanelHover : kPanel);
        DrawRectangleRoundedLinesEx(b, 0.35f, 8, 2.0f, hot ? GOLD : Color{110, 90, 70, 255});
        DrawCoin({b.x + 26, b.y + b.height / 2}, 12);
        DrawText(TextFormat("%d", profile_.Coins()), static_cast<int>(b.x + 46), static_cast<int>(b.y + 12), 22, GOLD);
        DrawText("Shop (S)", static_cast<int>(b.x + b.width - MeasureText("Shop (S)", 20) - 14), static_cast<int>(b.y + 13), 20, RAYWHITE);
    }

    for (int i = 0; i < WorldCount; ++i)
    {
        const Rectangle r = WorldCardRect(i);
        const bool sel = i == selectedWorld_;
        const int cx = static_cast<int>(r.x + r.width / 2);

        if (i > 0 && WorldUnlocked(i))
        {
            // Unlocked by beating the previous boss, but not built yet.
            const Color garden{52, 78, 56, 255};
            DrawRectangleRounded(r, 0.12f, 8, sel ? Color{64, 96, 68, 255} : garden);
            DrawRectangleRoundedLinesEx(r, 0.12f, 8, sel ? 3.0f : 2.0f, sel ? GOLD : Color{90, 130, 96, 255});
            DrawCentered(TextFormat("WORLD %d", i + 1), cx, static_cast<int>(r.y + 22), 22, Color{170, 230, 170, 255});
            DrawOpenPadlock({r.x + r.width / 2, r.y + r.height / 2 - 20}, 2.0f, Color{170, 230, 170, 255});
            DrawCentered(kWorldNames[i], cx, static_cast<int>(r.y + 205), 26, RAYWHITE);
            DrawCentered("Unlocked!", cx, static_cast<int>(r.y + 240), 20, Color{130, 225, 150, 255});
            DrawCentered("Coming soon", cx, static_cast<int>(r.y + 268), 18, LIGHTGRAY);
            continue;
        }
        if (i > 0)
        {
            // Locked world: blacked out.
            DrawRectangleRounded(r, 0.12f, 8, kLocked);
            DrawRectangleRoundedLinesEx(r, 0.12f, 8, sel ? 3.0f : 2.0f, sel ? Color{90, 84, 78, 255} : kLockedEdge);
            DrawCentered(TextFormat("WORLD %d", i + 1), cx, static_cast<int>(r.y + 22), 22, {70, 65, 60, 255});
            DrawPadlock({r.x + r.width / 2, r.y + r.height / 2 - 10}, 2.0f, {70, 65, 60, 255});
            DrawCentered("Coming soon", cx, static_cast<int>(r.y + r.height - 60), 20, {110, 104, 98, 255});
            continue;
        }

        // World 1
        DrawRectangleRounded(r, 0.12f, 8, sel ? kPanelHover : kPanel);
        if (sel) DrawRectangleRoundedLinesEx(r, 0.12f, 8, 3.0f, GOLD);
        DrawCentered("WORLD 1", cx, static_cast<int>(r.y + 22), 22, GOLD);

        // Mini level preview: a checkerboard room with walls, cheese, a cat and a mouse.
        const Rectangle art{r.x + 20, r.y + 60, r.width - 40, 130};
        DrawRectangleRec(art, {58, 52, 48, 255});
        for (int ty = 0; ty < 5; ++ty)
            for (int tx = 0; tx < 6; ++tx)
                if ((tx + ty) % 2 == 0) DrawRectangle(static_cast<int>(art.x + tx * 26.7f), static_cast<int>(art.y + ty * 26), 27, 26, {64, 57, 52, 255});
        DrawRectangle(static_cast<int>(art.x), static_cast<int>(art.y), static_cast<int>(art.width), 10, {92, 70, 52, 255});
        DrawRectangle(static_cast<int>(art.x + 70), static_cast<int>(art.y + 10), 12, 60, {92, 70, 52, 255});
        DrawCheeseSprite(CheeseKind::Cheddar, {art.x + 125, art.y + 42}, -0.6f, 1.0f);
        MouseLook mouse;
        mouse.pos = {art.x + 40, art.y + 98};
        mouse.facing = {0.8f, -0.6f};
        mouse.scale = 0.9f;
        DrawMouseSprite(mouse);
        CatLook cat;
        cat.pos = {art.x + 128, art.y + 102};
        cat.facing = {-1, 0};
        cat.scale = 0.85f;
        DrawCatSprite(cat);

        DrawCentered(kWorldNames[0], cx, static_cast<int>(r.y + 205), 26, RAYWHITE);
        const int cleared = ClearedCount();
        const int playable = std::min(static_cast<int>(levels_.size()), SlotsPerWorld);
        DrawCentered(TextFormat("%d / %d cleared", cleared, playable), cx, static_cast<int>(r.y + 240), 18, LIGHTGRAY);
        if (BossBeaten()) DrawCentered("Boss defeated!", cx, static_cast<int>(r.y + 290), 16, GOLD);
        const Rectangle bar{r.x + 24, r.y + 270, r.width - 48, 10};
        DrawRectangleRounded(bar, 1.0f, 6, {40, 34, 30, 255});
        if (playable > 0 && cleared > 0)
            DrawRectangleRounded({bar.x, bar.y, bar.width * cleared / playable, bar.height}, 1.0f, 6, kCleared);
    }

    DrawCentered("Left/Right or mouse to choose   -   Enter or click to play   -   S: shop   -   Esc to quit", w / 2, h - 40, 18, GRAY);

    if (toastTimer_ > 0.0f)
    {
        const char* msg = TextFormat("World %d: %s is still being built. Coming soon!", selectedWorld_ + 1, kWorldNames[selectedWorld_]);
        const int tw = MeasureText(msg, 22) + 40;
        const float a = std::min(1.0f, toastTimer_);
        DrawRectangleRounded({w / 2.0f - tw / 2.0f, 540, static_cast<float>(tw), 44}, 0.4f, 8, Fade(BLACK, 0.8f * a));
        DrawCentered(msg, w / 2, 551, 22, Fade(Color{170, 230, 170, 255}, a));
    }
}

// ---------------------------------------------------------------- Level select

Rectangle App::BossSlotRect() const
{
    const Rectangle last = LevelSlotRect(GridColumns - 1);
    const int rows = SlotsPerWorld / GridColumns;
    return {last.x + last.width + BossGap, static_cast<float>(GridTop), static_cast<float>(BossWidth),
            static_cast<float>(rows * SlotHeight + (rows - 1) * SlotGap)};
}

Rectangle App::LevelSlotRect(int index) const
{
    const int grid = GridColumns * SlotWidth + (GridColumns - 1) * SlotGap;
    const int total = grid + (HasBoss() ? BossGap + BossWidth : 0);
    const int col = index % GridColumns;
    const int row = index / GridColumns;
    return {static_cast<float>(GetScreenWidth() / 2 - total / 2 + col * (SlotWidth + SlotGap)),
            static_cast<float>(GridTop + row * (SlotHeight + SlotGap)), static_cast<float>(SlotWidth), static_cast<float>(SlotHeight)};
}

void App::UpdateLevels()
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        screen_ = Screen::Worlds;
        return;
    }

    // Arrow keys move around the 5x5 grid; Right from the last column reaches the boss slot.
    const bool right = IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D);
    const bool left = IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A);
    if (selectedSlot_ == BossSlot)
    {
        if (left) selectedSlot_ = lastGridSlot_;
    }
    else
    {
        const int col = selectedSlot_ % GridColumns;
        if (right && col == GridColumns - 1 && HasBoss())
        {
            lastGridSlot_ = selectedSlot_;
            selectedSlot_ = BossSlot;
        }
        else if (right && col < GridColumns - 1) ++selectedSlot_;
        else if (left && col > 0) --selectedSlot_;
        else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) selectedSlot_ = std::min(selectedSlot_ + GridColumns, SlotsPerWorld - 1);
        else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) selectedSlot_ = std::max(selectedSlot_ - GridColumns, 0);
    }

    bool clicked = false;
    for (int i = 0; i < SlotsPerWorld; ++i)
    {
        if (!CheckCollisionPointRec(GetMousePosition(), LevelSlotRect(i))) continue;
        clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        if (clicked || MouseMoved()) selectedSlot_ = i;
    }
    if (HasBoss() && CheckCollisionPointRec(GetMousePosition(), BossSlotRect()))
    {
        clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        if (clicked || MouseMoved()) selectedSlot_ = BossSlot;
    }

    if (IsUnlocked(selectedSlot_) && (clicked || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))) StartLevel(selectedSlot_);
}

void App::DrawLevels() const
{
    const int w = GetScreenWidth();
    const int h = GetScreenHeight();
    DrawBackground();

    DrawCentered(TextFormat("World 1: %s", kWorldNames[0]), w / 2, 40, 48, GOLD);
    DrawText("Esc: back", 24, 24, 20, GRAY);

    for (int i = 0; i < SlotsPerWorld; ++i)
    {
        const Rectangle r = LevelSlotRect(i);
        const bool sel = i == selectedSlot_;
        const int cx = static_cast<int>(r.x + r.width / 2);

        if (i < static_cast<int>(levels_.size()) && !IsUnlocked(i))
        {
            // Built but not unlocked yet: beat the previous level.
            DrawRectangleRounded(r, 0.2f, 8, Color{46, 40, 36, 255});
            DrawRectangleRoundedLinesEx(r, 0.2f, 8, sel ? 3.0f : 1.5f, sel ? Color{150, 130, 100, 255} : Color{70, 62, 56, 255});
            DrawText(TextFormat("%d", i + 1), static_cast<int>(r.x + 10), static_cast<int>(r.y + 8), 18, Color{130, 120, 110, 255});
            DrawPadlock({r.x + r.width / 2, r.y + r.height / 2 + 2}, 1.1f, Color{130, 120, 110, 255});
            continue;
        }
        if (i >= static_cast<int>(levels_.size()))
        {
            DrawRectangleRounded(r, 0.2f, 8, kLocked);
            DrawRectangleRoundedLinesEx(r, 0.2f, 8, sel ? 3.0f : 1.5f, sel ? Color{90, 84, 78, 255} : kLockedEdge);
            DrawText(TextFormat("%d", i + 1), static_cast<int>(r.x + 10), static_cast<int>(r.y + 8), 18, {70, 65, 60, 255});
            DrawPadlock({r.x + r.width / 2, r.y + r.height / 2 + 2}, 1.1f, {70, 65, 60, 255});
            continue;
        }

        const LevelEntry& e = levels_[i];
        const bool done = e.bestTime >= 0.0f;
        DrawRectangleRounded(r, 0.2f, 8, sel ? kPanelHover : kPanel);
        if (sel) DrawRectangleRoundedLinesEx(r, 0.2f, 8, 3.0f, GOLD);
        DrawCentered(TextFormat("%d", i + 1), cx, static_cast<int>(r.y + 10), 36, done ? Color{130, 225, 150, 255} : RAYWHITE);
        if (done)
            DrawCentered(FormatTime(e.bestTime).c_str(), cx, static_cast<int>(r.y + 52), 18, Color{130, 225, 150, 255});
        else
            DrawCentered("new", cx, static_cast<int>(r.y + 52), 18, GRAY);
    }

    if (HasBoss()) DrawBossSlot();

    // Info panel for the highlighted slot, spanning the grid (and boss slot).
    const float panelX = LevelSlotRect(0).x;
    const float panelRight = HasBoss() ? BossSlotRect().x + BossWidth : LevelSlotRect(GridColumns - 1).x + SlotWidth;
    const Rectangle panel{panelX, 590, panelRight - panelX, 80};
    DrawRectangleRounded(panel, 0.2f, 8, Fade(BLACK, 0.35f));
    if (selectedSlot_ == BossSlot && HasBoss())
    {
        const LevelEntry& e = levels_[BossSlot];
        if (!IsUnlocked(BossSlot))
        {
            DrawText("??? Secret level", static_cast<int>(panel.x + 20), static_cast<int>(panel.y + 12), 26, Color{150, 140, 130, 255});
            DrawText(TextFormat("Beat all %d levels to reveal it  (%d / %d)", SlotsPerWorld, ClearedCount(), SlotsPerWorld),
                     static_cast<int>(panel.x + 22), static_cast<int>(panel.y + 48), 20, Color{200, 170, 110, 255});
        }
        else
        {
            DrawText(TextFormat("BOSS: %s", e.name.c_str()), static_cast<int>(panel.x + 20), static_cast<int>(panel.y + 12), 26, Color{255, 150, 130, 255});
            DrawText("Trick Sir Pounce into pouncing into walls or furniture 3 times", static_cast<int>(panel.x + 22),
                     static_cast<int>(panel.y + 50), 18, LIGHTGRAY);
            const char* best = e.bestTime >= 0.0f ? TextFormat("Best %s", FormatTime(e.bestTime).c_str()) : "Not defeated";
            DrawText(best, static_cast<int>(panel.x + panel.width) - MeasureText(best, 22) - 20, static_cast<int>(panel.y + 14), 22,
                     e.bestTime >= 0.0f ? Color{130, 225, 150, 255} : GRAY);
        }
    }
    else if (selectedSlot_ < static_cast<int>(levels_.size()) && !IsUnlocked(selectedSlot_))
    {
        DrawText(TextFormat("Level %d: %s", selectedSlot_ + 1, levels_[selectedSlot_].name.c_str()), static_cast<int>(panel.x + 20),
                 static_cast<int>(panel.y + 12), 26, Color{150, 140, 130, 255});
        DrawText(TextFormat("Beat level %d to unlock", selectedSlot_), static_cast<int>(panel.x + 22), static_cast<int>(panel.y + 48), 20,
                 Color{200, 170, 110, 255});
    }
    else if (selectedSlot_ < static_cast<int>(levels_.size()))
    {
        const LevelEntry& e = levels_[selectedSlot_];
        DrawText(TextFormat("Level %d: %s", selectedSlot_ + 1, e.name.c_str()), static_cast<int>(panel.x + 20), static_cast<int>(panel.y + 12), 26, RAYWHITE);
        float x = panel.x + 22;
        for (int k = 0; k < CatKindCount; ++k)
        {
            if (!e.hasKind[k]) continue;
            const CatStats& stats = GetCatStats(static_cast<CatKind>(k));
            DrawCircleV({x, panel.y + 58}, 7, stats.fur);
            DrawText(stats.name, static_cast<int>(x + 12), static_cast<int>(panel.y + 50), 18, LIGHTGRAY);
            x += 30 + MeasureText(stats.name, 18);
        }
        const char* best = e.bestTime >= 0.0f ? TextFormat("Best %s", FormatTime(e.bestTime).c_str()) : "Not cleared yet";
        DrawText(best, static_cast<int>(panel.x + panel.width) - MeasureText(best, 22) - 20, static_cast<int>(panel.y + 29), 22,
                 e.bestTime >= 0.0f ? Color{130, 225, 150, 255} : GRAY);
    }
    else
    {
        DrawText(TextFormat("Level %d", selectedSlot_ + 1), static_cast<int>(panel.x + 20), static_cast<int>(panel.y + 12), 26, {110, 104, 98, 255});
        DrawText("Coming soon", static_cast<int>(panel.x + 22), static_cast<int>(panel.y + 48), 20, {110, 104, 98, 255});
    }

    DrawCentered("Arrows or mouse to choose   -   Enter or click to play", w / 2, h - 30, 18, GRAY);
}

// ---------------------------------------------------------------- Shop

namespace
{
constexpr int ShopCardW = 230;
constexpr int ShopCardH = 350;
constexpr int ShopCardGap = 24;
constexpr int ShopCardTop = 165;
constexpr int ShopSlots = 4; // the real skins, then "coming soon" placeholders
} // namespace

Rectangle App::ShopButtonRect() const
{
    return {GetScreenWidth() - 220.0f, 24.0f, 196.0f, 48.0f};
}

Rectangle App::ShopCardRect(int index) const
{
    const int total = ShopSlots * ShopCardW + (ShopSlots - 1) * ShopCardGap;
    const int x = GetScreenWidth() / 2 - total / 2 + index * (ShopCardW + ShopCardGap);
    return {static_cast<float>(x), static_cast<float>(ShopCardTop), static_cast<float>(ShopCardW), static_cast<float>(ShopCardH)};
}

void App::ActivateShopCard(int index)
{
    if (index < 0 || index >= MouseSkinCount) return;
    const MouseSkin skin = static_cast<MouseSkin>(index);
    const SkinInfo& info = GetSkinInfo(skin);
    if (profile_.Equipped() == skin) return; // already wearing it

    if (profile_.Owns(skin))
    {
        profile_.Equip(skin);
        shopToast_ = std::string("Equipped ") + info.name;
        shopToastGood_ = true;
    }
    else if (profile_.Buy(skin))
    {
        profile_.Equip(skin);
        shopToast_ = std::string("Bought ") + info.name + "! You're wearing it now.";
        shopToastGood_ = true;
    }
    else
    {
        shopToast_ = TextFormat("Not enough coins - you need %d more. Beat levels to earn cheese coins!", info.price - profile_.Coins());
        shopToastGood_ = false;
    }
    shopToastTimer_ = 3.0f;
    profile_.Save();
}

void App::UpdateShop()
{
    shopToastTimer_ = std::max(0.0f, shopToastTimer_ - GetFrameTime());
    if (IsKeyPressed(KEY_ESCAPE))
    {
        screen_ = Screen::Worlds;
        return;
    }
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) shopSel_ = std::min(shopSel_ + 1, MouseSkinCount - 1);
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) shopSel_ = std::max(shopSel_ - 1, 0);

    bool clicked = false;
    for (int i = 0; i < MouseSkinCount; ++i)
    {
        if (!CheckCollisionPointRec(GetMousePosition(), ShopCardRect(i))) continue;
        clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        if (clicked || MouseMoved()) shopSel_ = i;
    }
    if (clicked || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) ActivateShopCard(shopSel_);
}

void App::DrawShop() const
{
    const int w = GetScreenWidth();
    const int h = GetScreenHeight();
    const float t = static_cast<float>(GetTime());
    DrawBackground();

    DrawCentered("Shop", w / 2 + 3, 34, 64, {0, 0, 0, 120});
    DrawCentered("Shop", w / 2, 31, 64, GOLD);
    DrawCentered("Mouse skins", w / 2, 108, 26, LIGHTGRAY);
    DrawText("Esc: back", 24, 24, 20, GRAY);

    // Coin balance, top right.
    {
        const Rectangle b = ShopButtonRect();
        DrawRectangleRounded(b, 0.35f, 8, kPanel);
        DrawRectangleRoundedLinesEx(b, 0.35f, 8, 2.0f, Color{110, 90, 70, 255});
        DrawCoin({b.x + 26, b.y + b.height / 2}, 12);
        DrawText(TextFormat("%d", profile_.Coins()), static_cast<int>(b.x + 46), static_cast<int>(b.y + 12), 22, GOLD);
        DrawText("coins", static_cast<int>(b.x + b.width - MeasureText("coins", 20) - 14), static_cast<int>(b.y + 13), 20, LIGHTGRAY);
    }

    for (int i = 0; i < ShopSlots; ++i)
    {
        const Rectangle r = ShopCardRect(i);
        const int cx = static_cast<int>(r.x + r.width / 2);

        if (i >= MouseSkinCount)
        {
            DrawRectangleRounded(r, 0.08f, 8, kLocked);
            DrawRectangleRoundedLinesEx(r, 0.08f, 8, 1.5f, kLockedEdge);
            DrawCentered("?", cx, static_cast<int>(r.y + 90), 110, {56, 51, 47, 255});
            DrawCentered("More skins", cx, static_cast<int>(r.y + r.height - 78), 20, {110, 104, 98, 255});
            DrawCentered("coming soon", cx, static_cast<int>(r.y + r.height - 52), 20, {110, 104, 98, 255});
            continue;
        }

        const MouseSkin skin = static_cast<MouseSkin>(i);
        const SkinInfo& info = GetSkinInfo(skin);
        const bool sel = i == shopSel_;
        const bool owned = profile_.Owns(skin);
        const bool worn = profile_.Equipped() == skin;
        const bool affordable = profile_.Coins() >= info.price;

        DrawRectangleRounded(r, 0.08f, 8, sel ? kPanelHover : kPanel);
        DrawRectangleRoundedLinesEx(r, 0.08f, 8, sel ? 3.0f : 1.5f, sel ? GOLD : (worn ? Color{90, 170, 110, 255} : Color{100, 84, 68, 255}));
        if (worn) DrawCentered("EQUIPPED", cx, static_cast<int>(r.y + 14), 18, Color{130, 225, 150, 255});

        // Showroom: the mouse trots on the spot, turning slowly so you can see the whole outfit.
        const Rectangle stage{r.x + 16, r.y + 42, r.width - 32, 152};
        DrawRectangleRounded(stage, 0.12f, 8, {52, 45, 40, 255});
        const float a = t * 0.9f + i;
        MouseLook look;
        look.pos = {r.x + r.width / 2, stage.y + stage.height / 2};
        look.facing = {std::cos(a), std::sin(a)};
        look.moving = true;
        look.walkPhase = t * 9.0f;
        look.scale = 2.9f;
        look.skin = skin;
        BeginScissorMode(static_cast<int>(stage.x), static_cast<int>(stage.y), static_cast<int>(stage.width), static_cast<int>(stage.height));
        DrawMouseSprite(look);
        EndScissorMode();

        DrawCentered(info.name, cx, static_cast<int>(r.y + 208), 26, RAYWHITE);
        DrawCentered(info.line1, cx, static_cast<int>(r.y + 242), 17, LIGHTGRAY);
        DrawCentered(info.line2, cx, static_cast<int>(r.y + 264), 17, LIGHTGRAY);

        const Rectangle btn{r.x + 20, r.y + r.height - 60, r.width - 40, 42};
        if (worn)
        {
            DrawRectangleRounded(btn, 0.35f, 8, {38, 84, 52, 255});
            DrawCentered("Equipped", cx, static_cast<int>(btn.y + 10), 22, Color{130, 225, 150, 255});
        }
        else if (owned)
        {
            DrawRectangleRounded(btn, 0.35f, 8, sel ? Color{120, 96, 66, 255} : Color{92, 74, 56, 255});
            DrawCentered("Equip", cx, static_cast<int>(btn.y + 10), 22, RAYWHITE);
        }
        else
        {
            DrawRectangleRounded(btn, 0.35f, 8, affordable ? Color{112, 84, 24, 255} : Color{70, 48, 46, 255});
            const char* price = TextFormat("%d", info.price);
            const int pw = MeasureText(price, 22);
            DrawCoin({cx - pw / 2.0f - 6, btn.y + btn.height / 2}, 11);
            DrawText(price, cx - pw / 2 + 12, static_cast<int>(btn.y + 10), 22, affordable ? GOLD : Color{210, 120, 110, 255});
        }
    }

    if (shopToastTimer_ > 0.0f)
    {
        const int size = 22;
        const int tw = MeasureText(shopToast_.c_str(), size);
        const Rectangle box{w / 2.0f - tw / 2.0f - 20, ShopCardTop + ShopCardH + 26.0f, tw + 40.0f, 44.0f};
        DrawRectangleRounded(box, 0.35f, 8, Fade(BLACK, 0.7f * std::min(1.0f, shopToastTimer_)));
        DrawText(shopToast_.c_str(), static_cast<int>(box.x + 20), static_cast<int>(box.y + 11), size,
                 Fade(shopToastGood_ ? Color{130, 225, 150, 255} : Color{240, 140, 120, 255}, std::min(1.0f, shopToastTimer_)));
    }

    DrawCentered("Arrows or mouse to choose   -   Enter or click to buy / equip   -   Earn coins by getting cheese home", w / 2, h - 30, 18, GRAY);
}

// ---------------------------------------------------------------- Playing

void App::UpdatePlaying()
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        screen_ = Screen::Levels;
        selectedSlot_ = current_;
        return;
    }

    game_.Update(GetFrameTime());

    // Save the time once per win (R resets the game, which clears IsWon and allows the next win to save).
    if (game_.IsWon() && !winRecorded_)
    {
        winRecorded_ = true;
        levels_[current_].bestTime = game_.BestTime();
        WriteSave();
        profile_.AddCoins(game_.CoinsEarned());
        profile_.Save();
    }
    if (!game_.IsWon()) winRecorded_ = false;

    if (game_.IsWon() && IsKeyPressed(KEY_ENTER) && IsUnlocked(current_ + 1) && !levels_[current_].isBoss)
        StartLevel(current_ + 1);
}

void App::StartLevel(int index)
{
    const LevelEntry& entry = levels_[index];
    const bool hasNext = !entry.isBoss && index + 1 < static_cast<int>(levels_.size());
    const bool nextIsBoss = hasNext && levels_[index + 1].isBoss;
    if (!game_.Init(entry.path, entry.bestTime, hasNext, nextIsBoss)) return;
    game_.SetMouseSkin(profile_.Equipped());
    current_ = index;
    winRecorded_ = false;
    screen_ = Screen::Playing;
}

// ---------------------------------------------------------------- Shared

void App::Draw() const
{
    switch (screen_)
    {
    case Screen::Intro:   DrawIntro(); break;
    case Screen::Worlds:  DrawWorlds(); break;
    case Screen::Levels:  DrawLevels(); break;
    case Screen::Playing: game_.Draw(); break;
    case Screen::Shop:    DrawShop(); break;
    }
}

void App::DrawBackground() const
{
    // A dimmed checkerboard floor like the levels.
    ClearBackground({30, 27, 25, 255});
    const int w = GetScreenWidth();
    const int h = GetScreenHeight();
    for (int y = 0; y < h; y += Level::TileSize)
        for (int x = 0; x < w; x += Level::TileSize)
            if (((x + y) / Level::TileSize) % 2 == 0) DrawRectangle(x, y, Level::TileSize, Level::TileSize, {36, 32, 30, 255});
}

bool App::IsUnlocked(int index) const
{
    if (index < 0 || index >= static_cast<int>(levels_.size())) return false;
    if (levels_[index].isBoss) return ClearedCount() >= SlotsPerWorld; // secret: beat every level first
    return index == 0 || levels_[index - 1].bestTime >= 0.0f;
}

void App::DrawBossSlot() const
{
    const Rectangle r = BossSlotRect();
    const bool sel = selectedSlot_ == BossSlot;
    const int cx = static_cast<int>(r.x + r.width / 2);
    const float t = static_cast<float>(GetTime());

    if (!IsUnlocked(BossSlot))
    {
        // Secret: a dark door with a question mark.
        DrawRectangleRounded(r, 0.08f, 8, kLocked);
        DrawRectangleRoundedLinesEx(r, 0.08f, 8, sel ? 3.0f : 1.5f, sel ? Color{120, 100, 90, 255} : kLockedEdge);
        DrawCentered("SECRET", cx, static_cast<int>(r.y + 24), 24, Color{90, 82, 76, 255});
        DrawCentered("?", cx, static_cast<int>(r.y + r.height / 2 - 60), 120, Color{70, 64, 60, 255});
        DrawCentered(TextFormat("%d / %d", ClearedCount(), SlotsPerWorld), cx, static_cast<int>(r.y + r.height - 70), 22, Color{110, 104, 98, 255});
        DrawCentered("levels cleared", cx, static_cast<int>(r.y + r.height - 44), 16, Color{90, 84, 78, 255});
        return;
    }

    // Revealed: the boss's portrait.
    const LevelEntry& e = levels_[BossSlot];
    DrawRectangleRounded(r, 0.08f, 8, sel ? Color{86, 30, 34, 255} : Color{64, 22, 26, 255});
    const float glow = 0.5f + 0.5f * std::sin(t * 3.0f);
    DrawRectangleRoundedLinesEx(r, 0.08f, 8, sel ? 4.0f : 2.5f, sel ? Fade(GOLD, 0.6f + 0.4f * glow) : Color{150, 100, 50, 255});
    DrawCentered("BOSS", cx, static_cast<int>(r.y + 18), 34, GOLD);
    CatLook boss;
    boss.kind = CatKind::Boss;
    boss.pos = {r.x + r.width / 2, r.y + r.height / 2 - 10};
    boss.facing = {0, 1};
    boss.scale = 2.4f;
    boss.alert = sel;
    boss.pose = e.bestTime >= 0.0f ? CatPose::Stunned : CatPose::Normal; // already beaten: dazed
    DrawCatSprite(boss);
    DrawCentered("Sir Pounce", cx, static_cast<int>(r.y + r.height - 86), 24, RAYWHITE);
    if (e.bestTime >= 0.0f)
        DrawCentered(FormatTime(e.bestTime).c_str(), cx, static_cast<int>(r.y + r.height - 52), 20, Color{130, 225, 150, 255});
    else
        DrawCentered("Enter if you dare", cx, static_cast<int>(r.y + r.height - 52), 16, Color{230, 160, 140, 255});
}

int App::ClearedCount() const
{
    return static_cast<int>(std::count_if(levels_.begin(), levels_.end(), [](const LevelEntry& e) { return !e.isBoss && e.bestTime >= 0.0f; }));
}

void App::LoadSave()
{
    // Format: one line per completed level: "<level number> <best seconds>"
    std::ifstream in(savePath_);
    int number = 0;
    float time = 0.0f;
    while (in >> number >> time)
    {
        if (number >= 1 && number <= static_cast<int>(levels_.size())) levels_[number - 1].bestTime = time;
    }
}

void App::WriteSave() const
{
    std::ofstream out(savePath_);
    for (size_t i = 0; i < levels_.size(); ++i)
    {
        if (levels_[i].bestTime >= 0.0f) out << (i + 1) << ' ' << levels_[i].bestTime << '\n';
    }
}
