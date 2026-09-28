#include "Game.h"

#include <algorithm>
#include <cmath>

#include "raymath.h"

std::string FormatTime(float seconds)
{
    const int tenths = static_cast<int>(std::floor(seconds * 10.0f));
    return TextFormat("%d:%02d.%d", tenths / 600, (tenths / 10) % 60, tenths % 10);
}

bool Game::Init(const std::string& levelPath, float bestTime, bool hasNextLevel)
{
    if (!level_.LoadFromFile(levelPath)) return false;
    renderer_.Build(level_);
    bestTime_ = bestTime;
    hasNextLevel_ = hasNextLevel;
    Reset();
    return true;
}

void Game::Reset()
{
    player_ = Player(level_.PlayerStart());
    trail_.Reset(level_.PlayerStart());
    cheese_ = level_.CheeseSpawns();
    peppers_ = level_.PepperSpawns();
    boostTimer_ = 0.0f;
    flashPhase_ = 0.0f;
    totalCheese_ = static_cast<int>(cheese_.size());
    carried_ = 0;
    stored_ = 0;
    hiddenIn_ = -1;
    ignoreHole_ = -1;
    waitForKeyRelease_ = false;

    cats_.clear();
    for (const CatSpawn& spawn : level_.Cats()) cats_.emplace_back(spawn.kind, spawn.route);
    catSeesPlayer_.assign(cats_.size(), false);

    elapsed_ = 0.0f;
    newBest_ = false;
    popupTimer_ = 0.0f;
    state_ = State::Playing;
}

void Game::Update(float dt)
{
    if (IsKeyPressed(KEY_F1)) debug_ = !debug_;
    if (IsKeyPressed(KEY_R))
    {
        Reset();
        return;
    }
    if (state_ != State::Playing) return;

    dt = std::min(dt, 1.0f / 30.0f); // avoid huge jumps if a frame stalls (e.g. dragging the window)
    elapsed_ += dt;
    popupTimer_ -= dt;

    // Red pepper boost: flashes fast when fresh, slower and slower as it runs out.
    if (boostTimer_ > 0.0f)
    {
        const float flashesPerSecond = 1.5f + 6.5f * (boostTimer_ / PepperDuration);
        flashPhase_ += dt * flashesPerSecond * 2.0f * PI;
        boostTimer_ = std::max(0.0f, boostTimer_ - dt);
    }

    UpdatePlayer(dt);
    if (state_ == State::Playing) UpdateCats(dt);
}

void Game::UpdatePlayer(float dt)
{
    // Inside a mouse hole: Space travels to the linked hole, any movement key pops back out.
    if (IsHidden())
    {
        const MouseHole& hole = level_.Holes()[hiddenIn_];
        if (IsKeyPressed(KEY_SPACE) && hole.pair >= 0)
        {
            EnterHole(hole.pair);
            return;
        }
        // You usually arrive holding a direction key; let go first, then press again to leave.
        if (!Player::MovementKeyDown()) waitForKeyRelease_ = false;
        if (waitForKeyRelease_ || !Player::MovementKeyDown()) return;
        hiddenIn_ = -1; // step out (ignoreHole_ stops us from instantly re-entering)
    }

    const float cheeseFactor = std::max(MinSpeedFactor, 1.0f - SlowdownPerCheese * static_cast<float>(carried_));
    player_.SetSpeedMultiplier(cheeseFactor * (boostTimer_ > 0.0f ? PepperBoost : 1.0f));
    player_.Update(level_, dt);
    trail_.Record(player_.Position());

    // Pick up cheese: it joins the trail behind the mouse.
    const size_t before = cheese_.size();
    std::erase_if(cheese_, [&](const Vector2& c) {
        return CheckCollisionCircles(player_.Position(), player_.Radius(), c, CheeseRadius);
    });
    carried_ += static_cast<int>(before - cheese_.size());

    // Red pepper: eat it for a speed boost (a second pepper refills the timer).
    const size_t peppersBefore = peppers_.size();
    std::erase_if(peppers_, [&](const Vector2& p) {
        return CheckCollisionCircles(player_.Position(), player_.Radius(), p, PepperRadius);
    });
    if (peppers_.size() != peppersBefore)
    {
        boostTimer_ = PepperDuration;
        flashPhase_ = 0.0f;
    }

    const int tx = static_cast<int>(player_.Position().x) / Level::TileSize;
    const int ty = static_cast<int>(player_.Position().y) / Level::TileSize;

    // Mouse holes
    const int hole = level_.HoleAt(tx, ty);
    if (hole < 0)
        ignoreHole_ = -1; // stepped off the hole we left, so it works again
    else if (hole != ignoreHole_)
    {
        EnterHole(hole);
        return;
    }

    // Exit: only usable once every piece of cheese has been picked up.
    if (AllCheeseFound() && level_.At(tx, ty) == Tile::Exit)
    {
        StoreCheese(player_.Position());
        state_ = State::Won;
        newBest_ = bestTime_ < 0.0f || elapsed_ < bestTime_;
        if (newBest_) bestTime_ = elapsed_;
    }
}

void Game::EnterHole(int index)
{
    const MouseHole& hole = level_.Holes()[index];
    hiddenIn_ = index;
    ignoreHole_ = index;
    waitForKeyRelease_ = true;
    player_.SetPosition(hole.center);
    trail_.Reset(hole.center);
    StoreCheese(hole.center);
}

void Game::StoreCheese(Vector2 where)
{
    if (carried_ == 0) return;
    stored_ += carried_;
    popupCount_ = carried_;
    popupPos_ = where;
    popupTimer_ = 1.2f;
    carried_ = 0;
}

bool Game::IsFlashingRed() const
{
    return boostTimer_ > 0.0f && std::sin(flashPhase_) > -0.2f;
}

namespace
{
void DrawPepper(Vector2 c, float t)
{
    const float bob = std::sin(t * 3.0f + c.x) * 2.0f;
    const Color red{220, 40, 35, 255};
    const Color shine{255, 120, 100, 255};
    // Curved body made of shrinking circles, a highlight, and a green stem.
    DrawCircleV({c.x - 5, c.y - 3 + bob}, 8.0f, red);
    DrawCircleV({c.x + 1, c.y + 2 + bob}, 7.0f, red);
    DrawCircleV({c.x + 6, c.y + 7 + bob}, 5.0f, red);
    DrawCircleV({c.x + 10, c.y + 11 + bob}, 3.0f, red);
    DrawCircleV({c.x - 7, c.y - 5 + bob}, 2.5f, shine);
    DrawRectangleRounded({c.x - 12, c.y - 13 + bob, 8, 5}, 0.5f, 4, Color{60, 160, 60, 255});
    DrawLineEx({c.x - 10, c.y - 11 + bob}, {c.x - 14, c.y - 17 + bob}, 3.0f, Color{60, 160, 60, 255});
}
} // namespace

void Game::UpdateCats(float dt)
{
    // Cats: move, then check whether they can see or touch the mouse (never while it's hidden in a hole).
    for (size_t i = 0; i < cats_.size(); ++i)
    {
        Cat& cat = cats_[i];
        cat.Update(level_, dt);

        catSeesPlayer_[i] = false;
        if (IsHidden()) continue;

        // Caught by contact. A lunging cat's paws reach a little further; a dazed cat is harmless.
        const float reach = Cat::Radius + (cat.IsLunging() ? 6.0f : 0.0f);
        if (!cat.IsStunned() && CheckCollisionCircles(player_.Position(), player_.Radius(), cat.Position(), reach))
        {
            state_ = State::Caught;
            return;
        }

        catSeesPlayer_[i] = cat.CanSee(level_, player_.Position(), player_.Radius());
        if (catSeesPlayer_[i]) cat.Alert(level_, player_.Position());
    }
}

void Game::Draw() const
{
    renderer_.Draw(level_, AllCheeseFound());

    for (const Cat& cat : cats_) cat.DrawRoute();

    for (const Vector2& c : cheese_)
        DrawPoly(c, 3, CheeseRadius + 2.0f, -90.0f, GOLD);
    for (const Vector2& p : peppers_) DrawPepper(p, static_cast<float>(GetTime()));

    for (size_t i = 0; i < cats_.size(); ++i)
        cats_[i].Draw(level_, catSeesPlayer_[i]);
    if (debug_)
        for (const Cat& cat : cats_) cat.DrawDebug();

    // Carried cheese trailing behind the mouse (farthest first so nearer ones draw on top).
    const float t = static_cast<float>(GetTime());
    for (int i = carried_ - 1; i >= 0; --i)
    {
        const Vector2 p = trail_.PointBehind(player_.Position(), 30.0f + TrailSpacing * static_cast<float>(i));
        const float wobble = std::sin(t * 8.0f + static_cast<float>(i)) * 8.0f;
        DrawPoly(p, 3, CheeseRadius, -90.0f + wobble, GOLD);
        DrawPolyLines(p, 3, CheeseRadius, -90.0f + wobble, Color{200, 150, 20, 255});
    }

    if (IsHidden())
    {
        // Peeking eyes in the hole
        const Vector2 c = level_.Holes()[hiddenIn_].center;
        const float blink = std::fmod(t, 3.0f) < 0.12f ? 0.6f : 2.5f;
        DrawEllipse(static_cast<int>(c.x - 5), static_cast<int>(c.y), 2.5f, blink, RAYWHITE);
        DrawEllipse(static_cast<int>(c.x + 5), static_cast<int>(c.y), 2.5f, blink, RAYWHITE);
    }
    else
    {
        player_.Draw(IsFlashingRed());
    }

    if (popupTimer_ > 0.0f)
    {
        const float rise = (1.2f - popupTimer_) * 30.0f;
        const char* msg = TextFormat("+%d stored", popupCount_);
        DrawText(msg, static_cast<int>(popupPos_.x) - MeasureText(msg, 20) / 2, static_cast<int>(popupPos_.y - 34 - rise), 20,
                 Fade(GOLD, std::min(1.0f, popupTimer_)));
    }

    // HUD
    const int w = GetScreenWidth();
    const int h = GetScreenHeight();
    DrawRectangle(0, 0, w, 30, Fade(BLACK, 0.55f));
    DrawText(TextFormat("Stored %d/%d", stored_, totalCheese_), 12, 6, 20, RAYWHITE);
    if (carried_ > 0) DrawText(TextFormat("Carrying %d", carried_), 150, 6, 20, GOLD);
    DrawText(FormatTime(elapsed_).c_str(), 290, 6, 20, LIGHTGRAY);
    if (boostTimer_ > 0.0f)
    {
        // Pepper timer bar under the top bar.
        DrawRectangle(12, 34, 120, 8, Fade(BLACK, 0.6f));
        DrawRectangle(12, 34, static_cast<int>(120 * boostTimer_ / PepperDuration), 8, Color{235, 60, 50, 255});
        DrawText("SPEED!", 140, 30, 16, Color{255, 110, 90, 255});
    }

    const char* hint;
    Color hintColor = LIGHTGRAY;
    if (IsHidden())
    {
        const MouseHole& hole = level_.Holes()[hiddenIn_];
        hint = hole.pair >= 0 ? TextFormat("Hidden!  Space: travel to the other %s hole   Move: leave", HoleColorName(hole.color))
                              : "Hidden!  Move to leave";
        hintColor = HoleColor(hole.color);
    }
    else if (AllCheeseFound())
    {
        hint = "All cheese found! Get to the exit.";
        hintColor = GREEN;
    }
    else
    {
        hint = "Grab the cheese. Mouse holes hide you and store it.";
    }
    DrawText(hint, w / 2 + 40 - MeasureText(hint, 20) / 2, 6, 20, hintColor);
    const char* keys = "R: restart   Esc: menu";
    DrawText(keys, w - MeasureText(keys, 20) - 12, 6, 20, GRAY);
    if (debug_) DrawText("DEBUG (F1)", 12, h - 26, 20, SKYBLUE);

    // Level name, fading out over the first two seconds.
    if (state_ == State::Playing && elapsed_ < 2.0f && !level_.Name().empty())
    {
        const float alpha = std::clamp(2.0f - elapsed_, 0.0f, 1.0f);
        const char* name = level_.Name().c_str();
        DrawText(name, w / 2 - MeasureText(name, 48) / 2, h / 2 - 24, 48, Fade(RAYWHITE, alpha));
    }

    if (state_ == State::Won || state_ == State::Caught)
    {
        const bool won = state_ == State::Won;
        DrawRectangle(0, 0, w, h, Fade(BLACK, 0.6f));
        const char* msg = won ? "LEVEL COMPLETE!" : "CAUGHT!";
        DrawText(msg, w / 2 - MeasureText(msg, 60) / 2, h / 2 - 80, 60, won ? GOLD : RED);

        if (won)
        {
            const char* time = TextFormat("Time %s%s", FormatTime(elapsed_).c_str(), newBest_ ? "   New best!" : "");
            DrawText(time, w / 2 - MeasureText(time, 28) / 2, h / 2 - 5, 28, newBest_ ? GREEN : RAYWHITE);
            if (!newBest_)
            {
                const char* best = TextFormat("Best %s", FormatTime(bestTime_).c_str());
                DrawText(best, w / 2 - MeasureText(best, 22) / 2, h / 2 + 30, 22, LIGHTGRAY);
            }
        }
        else
        {
            const char* lost = TextFormat("You had %d of %d cheese stored", stored_, totalCheese_);
            DrawText(lost, w / 2 - MeasureText(lost, 24) / 2, h / 2 - 5, 24, LIGHTGRAY);
        }

        const char* sub = won ? (hasNextLevel_ ? "Enter: next level     R: replay     Esc: menu" : "You beat the last level!     R: replay     Esc: menu")
                              : "R: try again     Esc: menu";
        DrawText(sub, w / 2 - MeasureText(sub, 22) / 2, h / 2 + 70, 22, RAYWHITE);
    }
}
