#include "Game.h"

#include <algorithm>

bool Game::Init(const std::string& levelPath)
{
    if (!level_.LoadFromFile(levelPath)) return false;
    Reset();
    return true;
}

void Game::Reset()
{
    player_ = Player(level_.PlayerStart());
    cheese_ = level_.CheeseSpawns();
    totalCheese_ = static_cast<int>(cheese_.size());
    state_ = State::Playing;
}

void Game::Update(float dt)
{
    if (IsKeyPressed(KEY_R))
    {
        Reset();
        return;
    }
    if (state_ != State::Playing) return;

    dt = std::min(dt, 1.0f / 30.0f); // avoid huge jumps if a frame stalls (e.g. dragging the window)
    player_.Update(level_, dt);

    // Collect any cheese the player is touching.
    std::erase_if(cheese_, [&](const Vector2& c) {
        return CheckCollisionCircles(player_.Position(), player_.Radius(), c, CheeseRadius);
    });

    if (ExitOpen())
    {
        const int tx = static_cast<int>(player_.Position().x) / Level::TileSize;
        const int ty = static_cast<int>(player_.Position().y) / Level::TileSize;
        if (level_.At(tx, ty) == Tile::Exit) state_ = State::Won;
    }
}

void Game::Draw() const
{
    level_.Draw(ExitOpen());

    for (const Vector2& c : cheese_)
        DrawPoly(c, 3, CheeseRadius + 2.0f, -90.0f, GOLD);

    player_.Draw();

    // HUD
    const int collected = totalCheese_ - static_cast<int>(cheese_.size());
    DrawRectangle(0, 0, GetScreenWidth(), 30, Fade(BLACK, 0.55f));
    DrawText(TextFormat("Cheese: %d / %d", collected, totalCheese_), 12, 6, 20, RAYWHITE);
    const char* hint = ExitOpen() ? "Exit open! Get to the green hole." : "Collect all the cheese to open the exit";
    DrawText(hint, GetScreenWidth() / 2 - MeasureText(hint, 20) / 2, 6, 20, ExitOpen() ? GREEN : LIGHTGRAY);
    DrawText("R: restart", GetScreenWidth() - 110, 6, 20, GRAY);

    if (state_ == State::Won)
    {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.6f));
        const char* msg = "LEVEL COMPLETE!";
        DrawText(msg, GetScreenWidth() / 2 - MeasureText(msg, 60) / 2, GetScreenHeight() / 2 - 50, 60, GOLD);
        const char* sub = "Press R to play again";
        DrawText(sub, GetScreenWidth() / 2 - MeasureText(sub, 24) / 2, GetScreenHeight() / 2 + 20, 24, RAYWHITE);
    }
}
