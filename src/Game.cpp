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
    totalCheese_ = static_cast<int>(cheese_.size());
    carried_ = 0;
    stored_ = 0;
    hiddenIn_ = -1;
    ignoreHole_ = -1;
    waitForKeyRelease_ = false;

    cats_.clear();
    for (const CatSpawn& spawn : level_.Cats()) cats_.emplace_back(spawn.kind, spawn.route);
    catSeesPlayer_.assign(cats_.size(), false);

    detection_ = 0.0f;
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

    player_.SetSpeedMultiplier(std::max(MinSpeedFactor, 1.0f - SlowdownPerCheese * static_cast<float>(carried_)));
    player_.Update(level_, dt);
    trail_.Record(player_.Position());

    // Pick up cheese: it joins the trail behind the mouse.
    const size_t before = cheese_.size();
    std::erase_if(cheese_, [&](const Vector2& c) {
        return CheckCollisionCircles(player_.Position(), player_.Radius(), c, CheeseRadius);
    });
    carried_ += static_cast<int>(before - cheese_.size());

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

void Game::UpdateCats(float dt)
{
    // Cats: move, then check whether they can see or touch the player (never while hidden in a hole).
    float fillRate = 0.0f; // detection gained per second this frame
    for (size_t i = 0; i < cats_.size(); ++i)
    {
        Cat& cat = cats_[i];
        cat.Update(level_, dt);

        catSeesPlayer_[i] = false;
        if (IsHidden()) continue;

        if (CheckCollisionCircles(player_.Position(), player_.Radius(), cat.Position(), Cat::Radius))
            detection_ = 1.0f; // bumped right into a cat

        catSeesPlayer_[i] = cat.CanSee(level_, player_.Position(), player_.Radius());
        if (catSeesPlayer_[i])
        {
            cat.Alert(level_, player_.Position());
            // The closer the cat, the faster it recognizes you (1x at max range, 3x point-blank).
            const float dist = Vector2Distance(cat.Position(), player_.Position());
            const float closeness = 1.0f - std::clamp(dist / cat.ViewRange(), 0.0f, 1.0f);
            fillRate = std::max(fillRate, cat.DetectMultiplier() * (1.0f + 2.0f * closeness) / DetectTime);
        }
    }

    // Detection meter: fills while seen, drains once you break line of sight.
    if (fillRate > 0.0f)
        detection_ += fillRate * dt;
    else
        detection_ -= dt / (DetectTime * 2.0f);
    detection_ = std::clamp(detection_, 0.0f, 1.0f);

    if (detection_ >= 1.0f) state_ = State::Caught;
}

void Game::Draw() const
{
    level_.Draw(AllCheeseFound());

    for (const Cat& cat : cats_) cat.DrawRoute();

    for (const Vector2& c : cheese_)
        DrawPoly(c, 3, CheeseRadius + 2.0f, -90.0f, GOLD);

    for (size_t i = 0; i < cats_.size(); ++i)
        cats_[i].Draw(level_, catSeesPlayer_[i] ? detection_ : 0.0f, catSeesPlayer_[i]);
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
        player_.Draw();
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

    // Detection bar (only when it's not empty)
    if (detection_ > 0.0f)
    {
        const int barW = 200;
        const int barX = w / 2 - barW / 2;
        DrawRectangle(barX, 36, barW, 10, Fade(BLACK, 0.6f));
        DrawRectangle(barX, 36, static_cast<int>(barW * detection_), 10, ColorLerp(YELLOW, RED, detection_));
    }

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
