#include "App.h"

#include <fstream>

#include "raylib.h"

namespace
{
constexpr int RowWidth = 640;
constexpr int RowHeight = 62;
constexpr int RowGap = 12;
constexpr int ListTop = 200;
} // namespace

void App::Init()
{
    const std::string dir = GetApplicationDirectory();
    savePath_ = dir + "save.txt";

    // Find level1.txt, level2.txt, ... until one is missing, so new levels are picked up automatically.
    for (int i = 1;; ++i)
    {
        const std::string path = dir + TextFormat("assets/levels/level%d.txt", i);
        if (!FileExists(path.c_str())) break;

        Level level;
        if (!level.LoadFromFile(path)) continue;

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
    if (screen_ == Screen::Menu)
        UpdateMenu();
    else
        UpdatePlaying();
    return !quit_;
}

void App::UpdateMenu()
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        quit_ = true;
        return;
    }
    if (levels_.empty()) return;

    const int count = static_cast<int>(levels_.size());
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) selected_ = (selected_ + 1) % count;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) selected_ = (selected_ + count - 1) % count;

    // Mouse: hover to highlight, click to play.
    for (int i = 0; i < count; ++i)
    {
        if (CheckCollisionPointRec(GetMousePosition(), RowRect(i)))
        {
            selected_ = i;
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                StartLevel(i);
                return;
            }
        }
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) StartLevel(selected_);
}

void App::UpdatePlaying()
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        screen_ = Screen::Menu;
        selected_ = current_;
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

Rectangle App::RowRect(int index) const
{
    return {static_cast<float>(GetScreenWidth() / 2 - RowWidth / 2),
            static_cast<float>(ListTop + index * (RowHeight + RowGap)),
            static_cast<float>(RowWidth), static_cast<float>(RowHeight)};
}

void App::Draw() const
{
    if (screen_ == Screen::Playing)
        game_.Draw();
    else
        DrawMenu();
}

void App::DrawMenu() const
{
    const int w = GetScreenWidth();
    const int h = GetScreenHeight();

    // Background: a dimmed checkerboard floor like the levels.
    ClearBackground({30, 27, 25, 255});
    for (int y = 0; y < h; y += Level::TileSize)
        for (int x = 0; x < w; x += Level::TileSize)
            if (((x + y) / Level::TileSize) % 2 == 0) DrawRectangle(x, y, Level::TileSize, Level::TileSize, {36, 32, 30, 255});

    // Title
    const char* title = "MouseGame";
    DrawText(title, w / 2 - MeasureText(title, 80) / 2 + 4, 54, 80, {0, 0, 0, 120});
    DrawText(title, w / 2 - MeasureText(title, 80) / 2, 50, 80, GOLD);
    const char* tagline = "Sneak past the cats. Steal the cheese.";
    DrawText(tagline, w / 2 - MeasureText(tagline, 24) / 2, 140, 24, LIGHTGRAY);

    if (levels_.empty())
    {
        const char* msg = "No levels found in assets/levels/";
        DrawText(msg, w / 2 - MeasureText(msg, 24) / 2, ListTop, 24, RED);
        return;
    }

    // Level rows
    for (int i = 0; i < static_cast<int>(levels_.size()); ++i)
    {
        const LevelEntry& e = levels_[i];
        const Rectangle r = RowRect(i);
        const bool sel = i == selected_;
        const bool done = e.bestTime >= 0.0f;

        DrawRectangleRounded(r, 0.25f, 8, sel ? Color{92, 72, 52, 255} : Color{58, 50, 45, 235});
        if (sel) DrawRectangleRoundedLinesEx(r, 0.25f, 8, 3.0f, GOLD);

        // Number badge
        const Vector2 badge{r.x + 38, r.y + r.height / 2};
        DrawCircleV(badge, 21, done ? Color{60, 170, 90, 255} : Color{90, 80, 72, 255});
        const char* num = TextFormat("%d", i + 1);
        DrawText(num, static_cast<int>(badge.x) - MeasureText(num, 26) / 2, static_cast<int>(badge.y) - 13, 26, RAYWHITE);

        // Name
        DrawText(e.name.c_str(), static_cast<int>(r.x + 76), static_cast<int>(r.y + 10), 26, RAYWHITE);

        // Which cats live in this level
        float cx = r.x + 84;
        for (int k = 0; k < CatKindCount; ++k)
        {
            if (!e.hasKind[k]) continue;
            const CatStats& stats = GetCatStats(static_cast<CatKind>(k));
            DrawCircleV({cx, r.y + 46}, 7, stats.fur);
            DrawText(stats.name, static_cast<int>(cx + 12), static_cast<int>(r.y + 38), 16, LIGHTGRAY);
            cx += 24 + MeasureText(stats.name, 16);
        }

        // Best time
        const char* best = done ? TextFormat("Best %s", FormatTime(e.bestTime).c_str()) : "Not cleared";
        DrawText(best, static_cast<int>(r.x + r.width) - MeasureText(best, 22) - 20, static_cast<int>(r.y + r.height / 2 - 11), 22,
                 done ? Color{120, 220, 140, 255} : GRAY);
    }

    // Cat guide
    const int guideX = w / 2 - RowWidth / 2 + 20;
    int guideY = ListTop + static_cast<int>(levels_.size()) * (RowHeight + RowGap) + 6;
    for (int k = 0; k < CatKindCount; ++k)
    {
        const CatStats& stats = GetCatStats(static_cast<CatKind>(k));
        DrawCircleV({static_cast<float>(guideX + 8), static_cast<float>(guideY + 9)}, 8, stats.fur);
        DrawText(stats.name, guideX + 24, guideY, 18, RAYWHITE);
        DrawText(stats.description, guideX + 110, guideY, 18, LIGHTGRAY);
        guideY += 24;
    }

    const char* controls = "Up/Down or mouse to choose   -   Enter or click to play   -   Esc to quit";
    DrawText(controls, w / 2 - MeasureText(controls, 18) / 2, h - 34, 18, GRAY);
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
