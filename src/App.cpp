#include "App.h"

#include <algorithm>
#include <cmath>
#include <fstream>

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

void DrawPadlock(Vector2 c, float s, Color color)
{
    DrawRing({c.x, c.y - 4 * s}, 6 * s, 9 * s, 180.0f, 360.0f, 24, color);             // shackle
    DrawRectangleRounded({c.x - 12 * s, c.y - 4 * s, 24 * s, 18 * s}, 0.3f, 6, color); // body
    DrawCircleV({c.x, c.y + 4 * s}, 2.5f * s, kLocked);                                // keyhole
}

// Simple mouse illustration used on the intro screen.
void DrawMouseArt(Vector2 c, float s, float alpha)
{
    const Color body = Fade(LIGHTGRAY, alpha);
    const Color pink = Fade(PINK, alpha);
    DrawSplineSegmentBezierQuadratic({c.x - 38 * s, c.y + 8 * s}, {c.x - 80 * s, c.y + 40 * s}, {c.x - 70 * s, c.y - 10 * s}, 4 * s, pink); // tail
    DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y), 44 * s, 30 * s, body);
    DrawCircleV({c.x + 20 * s, c.y - 26 * s}, 17 * s, body); // ears
    DrawCircleV({c.x + 20 * s, c.y - 26 * s}, 11 * s, pink);
    DrawCircleV({c.x + 44 * s, c.y - 4 * s}, 22 * s, body); // head
    DrawCircleV({c.x + 50 * s, c.y - 10 * s}, 3.5f * s, Fade(BLACK, alpha)); // eye
    DrawCircleV({c.x + 66 * s, c.y}, 5 * s, pink); // nose
}

void DrawCheeseArt(Vector2 c, float s, float alpha)
{
    const Vector2 a{c.x - 34 * s, c.y + 22 * s}, b{c.x + 34 * s, c.y + 22 * s}, top{c.x + 20 * s, c.y - 26 * s};
    DrawTriangle(top, a, b, Fade(GOLD, alpha));
    DrawCircleV({c.x + 4 * s, c.y + 8 * s}, 6 * s, Fade(Color{200, 150, 20, 255}, alpha));
    DrawCircleV({c.x + 18 * s, c.y - 6 * s}, 4 * s, Fade(Color{200, 150, 20, 255}, alpha));
}
} // namespace

void App::Init()
{
    const std::string dir = GetApplicationDirectory();
    savePath_ = dir + "save_world1.txt"; // v2 levels (the house rooms); older saves are ignored

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
    ClearBackground({14, 12, 11, 255});

    // Fade in over 1.2s, hold, fade out over the last 1.2s.
    const float t = introTime_;
    const float alpha = std::clamp(std::min(t / 1.2f, (IntroLength - t) / 1.2f), 0.0f, 1.0f);
    const float drift = (1.0f - alpha) * 12.0f;

    // The mouse creeps toward the cheese.
    const float creep = std::min(t / IntroLength, 1.0f) * 60.0f;
    DrawMouseArt({w / 2.0f - 150 + creep, h / 2.0f - 70 + drift}, 1.0f, alpha);
    DrawCheeseArt({w / 2.0f + 130, h / 2.0f - 62 + drift}, 1.0f, alpha);

    DrawCentered("MouseGame", w / 2, static_cast<int>(h / 2.0f + 10 + drift), 90, Fade(GOLD, alpha));
    DrawCentered(kCredit, w / 2, static_cast<int>(h / 2.0f + 110 + drift), 26, Fade(LIGHTGRAY, alpha));
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

    // Only World 1 exists so far.
    if ((clicked || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) && selectedWorld_ == 0)
    {
        screen_ = Screen::Levels;
        selectedSlot_ = 0;
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

    for (int i = 0; i < WorldCount; ++i)
    {
        const Rectangle r = WorldCardRect(i);
        const bool sel = i == selectedWorld_;
        const int cx = static_cast<int>(r.x + r.width / 2);

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
        DrawPoly({art.x + 125, art.y + 45}, 3, 10, -90, GOLD);
        DrawCircleV({art.x + 40, art.y + 100}, 9, LIGHTGRAY);
        DrawCircleV({art.x + 34, art.y + 92}, 4, PINK);
        DrawCircleV({art.x + 46, art.y + 92}, 4, PINK);
        DrawCircleV({art.x + 128, art.y + 100}, 11, {225, 135, 50, 255});

        DrawCentered(kWorldNames[0], cx, static_cast<int>(r.y + 205), 26, RAYWHITE);
        const int cleared = ClearedCount();
        const int playable = static_cast<int>(levels_.size());
        DrawCentered(TextFormat("%d / %d cleared", cleared, playable), cx, static_cast<int>(r.y + 240), 18, LIGHTGRAY);
        const Rectangle bar{r.x + 24, r.y + 270, r.width - 48, 10};
        DrawRectangleRounded(bar, 1.0f, 6, {40, 34, 30, 255});
        if (playable > 0 && cleared > 0)
            DrawRectangleRounded({bar.x, bar.y, bar.width * cleared / playable, bar.height}, 1.0f, 6, kCleared);
    }

    DrawCentered("Left/Right or mouse to choose   -   Enter or click to play   -   Esc to quit", w / 2, h - 40, 18, GRAY);
}

// ---------------------------------------------------------------- Level select

Rectangle App::LevelSlotRect(int index) const
{
    const int total = GridColumns * SlotWidth + (GridColumns - 1) * SlotGap;
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

    // Arrow keys move around the 5x5 grid.
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) selectedSlot_ = std::min(selectedSlot_ + 1, SlotsPerWorld - 1);
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) selectedSlot_ = std::max(selectedSlot_ - 1, 0);
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) selectedSlot_ = std::min(selectedSlot_ + GridColumns, SlotsPerWorld - 1);
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) selectedSlot_ = std::max(selectedSlot_ - GridColumns, 0);

    bool clicked = false;
    for (int i = 0; i < SlotsPerWorld; ++i)
    {
        if (!CheckCollisionPointRec(GetMousePosition(), LevelSlotRect(i))) continue;
        clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        if (clicked || MouseMoved()) selectedSlot_ = i;
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

    // Info panel for the highlighted slot
    const Rectangle panel{static_cast<float>(w / 2 - 357), 590, 714, 80};
    DrawRectangleRounded(panel, 0.2f, 8, Fade(BLACK, 0.35f));
    if (selectedSlot_ < static_cast<int>(levels_.size()) && !IsUnlocked(selectedSlot_))
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
    }
    if (!game_.IsWon()) winRecorded_ = false;

    if (game_.IsWon() && IsKeyPressed(KEY_ENTER) && current_ + 1 < static_cast<int>(levels_.size()))
        StartLevel(current_ + 1);
}

void App::StartLevel(int index)
{
    const LevelEntry& entry = levels_[index];
    const bool hasNext = index + 1 < static_cast<int>(levels_.size());
    if (!game_.Init(entry.path, entry.bestTime, hasNext)) return;
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
    return index == 0 || levels_[index - 1].bestTime >= 0.0f;
}

int App::ClearedCount() const
{
    return static_cast<int>(std::count_if(levels_.begin(), levels_.end(), [](const LevelEntry& e) { return e.bestTime >= 0.0f; }));
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
