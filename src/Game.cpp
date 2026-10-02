#include "Game.h"

#include <algorithm>
#include <cmath>

#include "Sprites.h"
#include "Viewport.h"
#include "raymath.h"

std::string FormatTime(float seconds)
{
    const int tenths = static_cast<int>(std::floor(seconds * 10.0f));
    return TextFormat("%d:%02d.%d", tenths / 600, (tenths / 10) % 60, tenths % 10);
}

bool Game::Init(const std::string& levelPath, float bestTime, bool hasNextLevel, bool nextIsBoss)
{
    if (!level_.LoadFromFile(levelPath)) return false;
    renderer_.Build(level_);
    bestTime_ = bestTime;
    hasNextLevel_ = hasNextLevel;
    nextIsBoss_ = nextIsBoss;
    Reset();
    return true;
}

void Game::Reset()
{
    player_ = Player(level_.PlayerStart());
    trail_.Reset(level_.PlayerStart());
    cheese_.clear();
    for (size_t i = 0; i < level_.CheeseSpawns().size(); ++i) cheese_.push_back({level_.CheeseSpawns()[i], level_.CheeseKinds()[i]});
    carriedKinds_.clear();
    carriedWeight_ = 0;
    peppers_ = level_.PepperSpawns();
    boostTimer_ = 0.0f;
    flashPhase_ = 0.0f;
    totalCheese_ = static_cast<int>(cheese_.size());
    carried_ = 0;
    stored_ = 0;
    storedCoins_ = 0;
    firstClear_ = false;
    hiddenIn_ = -1;
    ignoreHole_ = -1;
    waitForKeyRelease_ = false;

    cats_.clear();
    bossIndex_ = -1;
    for (const CatSpawn& spawn : level_.Cats())
    {
        if (GetCatStats(spawn.kind).boss) bossIndex_ = static_cast<int>(cats_.size());
        cats_.emplace_back(spawn.kind, spawn.route);
    }
    goldenDropped_ = false;
    bossHitsShown_ = 0;
    bossFlash_ = 0.0f;
    catSeesPlayer_.assign(cats_.size(), false);
    catSound_.assign(cats_.size(), CatSoundState{});
    stepDistance_ = 0.0f;
    stepFlip_ = false;
    exitWasOpen_ = AllCheeseFound();
    Audio::DuckMusic(0.0f);

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
    if (state_ == State::Playing) UpdateBoss();
    bossFlash_ = std::max(0.0f, bossFlash_ - dt);

    // The exit lights up the moment the last cheese is taken.
    const bool exitOpen = AllCheeseFound();
    if (exitOpen && !exitWasOpen_ && state_ == State::Playing) Audio::Play(Sfx::ExitOpen);
    exitWasOpen_ = exitOpen;
}

void Game::UpdateBoss()
{
    if (bossIndex_ < 0) return;
    const Cat& boss = cats_[bossIndex_];
    if (boss.BossHits() > bossHitsShown_)
    {
        bossHitsShown_ = boss.BossHits();
        bossFlash_ = 1.2f;
    }
    if (boss.IsKnockedOut() && !goldenDropped_)
    {
        // Drop the Golden Cheese on an open tile next to the fallen boss.
        goldenDropped_ = true;
        const int bx = static_cast<int>(boss.Position().x) / Level::TileSize;
        const int by = static_cast<int>(boss.Position().y) / Level::TileSize;
        Vector2 spot = boss.Position();
        static const int offsets[][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}, {0, 2}, {0, -2}};
        for (const auto& o : offsets)
        {
            const int dx = o[0], dy = o[1];
            if (!level_.IsWall(bx + dx, by + dy) && level_.At(bx + dx, by + dy) != Tile::Hole)
            {
                spot = {(bx + dx + 0.5f) * Level::TileSize, (by + dy + 0.5f) * Level::TileSize};
                break;
            }
        }
        cheese_.push_back({spot, CheeseKind::Golden});
        ++totalCheese_;
    }
}

void Game::UpdatePlayer(float dt)
{
    // Inside a mouse hole: Space travels to the linked hole, any movement key pops back out.
    if (IsHidden())
    {
        const MouseHole& hole = level_.Holes()[hiddenIn_];
        if (IsKeyPressed(KEY_SPACE) && hole.pair >= 0)
        {
            EnterHole(hole.pair, true);
            return;
        }
        // You usually arrive holding a direction key; let go first, then press again to leave.
        if (!Player::MovementKeyDown()) waitForKeyRelease_ = false;
        if (waitForKeyRelease_ || !Player::MovementKeyDown()) return;
        hiddenIn_ = -1; // step out (ignoreHole_ stops us from instantly re-entering)
        Audio::Play(Sfx::HoleExit);
    }

    const float cheeseFactor = std::max(MinSpeedFactor, 1.0f - SlowdownPerCheese * static_cast<float>(carriedWeight_));
    player_.SetSpeedMultiplier(cheeseFactor * (boostTimer_ > 0.0f ? PepperBoost : 1.0f));
    const Vector2 before = player_.Position();
    player_.Update(level_, dt);

    // Tiny footsteps, one every few pixels walked (so a pepper boost pitter-patters faster).
    stepDistance_ += Vector2Distance(before, player_.Position());
    if (stepDistance_ >= StepSpacing)
    {
        stepDistance_ -= StepSpacing;
        stepFlip_ = !stepFlip_;
        Audio::Play(stepFlip_ ? Sfx::Step1 : Sfx::Step2, 0.8f, 0.0f, 0.95f + 0.1f * static_cast<float>(GetRandomValue(0, 100)) / 100.0f);
    }
    trail_.Record(player_.Position());

    // Pick up cheese: it joins the trail behind the mouse.
    std::erase_if(cheese_, [&](const CheesePiece& c) {
        if (!CheckCollisionCircles(player_.Position(), player_.Radius(), c.pos, CheeseRadius)) return false;
        carriedKinds_.push_back(c.kind);
        carriedWeight_ += GetCheeseInfo(c.kind).weight;
        ++carried_;
        // Each piece in a row is a little higher, so a trail of cheese plays a rising run.
        if (c.kind == CheeseKind::Golden)
            Audio::Play(Sfx::GoldenCheese);
        else
            Audio::Play(Sfx::Cheese, 1.0f, 0.0f, std::min(1.6f, 1.0f + 0.05f * static_cast<float>(carried_ - 1)));
        return true;
    });

    // Red pepper: eat it for a speed boost (a second pepper refills the timer).
    const size_t peppersBefore = peppers_.size();
    std::erase_if(peppers_, [&](const Vector2& p) {
        return CheckCollisionCircles(player_.Position(), player_.Radius(), p, PepperRadius);
    });
    if (peppers_.size() != peppersBefore)
    {
        boostTimer_ = PepperDuration;
        flashPhase_ = 0.0f;
        Audio::Play(Sfx::Pepper);
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
        StoreCheese(player_.Position(), true);
        state_ = State::Won;
        Audio::Play(Sfx::Win);
        Audio::DuckMusic(4.0f);
        firstClear_ = bestTime_ < 0.0f;
        newBest_ = bestTime_ < 0.0f || elapsed_ < bestTime_;
        if (newBest_) bestTime_ = elapsed_;
    }
}

void Game::EnterHole(int index, bool teleport)
{
    const MouseHole& hole = level_.Holes()[index];
    Audio::Play(teleport ? Sfx::HoleTeleport : Sfx::HoleEnter);
    hiddenIn_ = index;
    ignoreHole_ = index;
    waitForKeyRelease_ = true;
    player_.SetPosition(hole.center);
    trail_.Reset(hole.center);
    StoreCheese(hole.center);
}

void Game::StoreCheese(Vector2 where, bool silent)
{
    if (carried_ == 0) return;
    if (!silent) Audio::Play(Sfx::Store);
    stored_ += carried_;
    for (CheeseKind kind : carriedKinds_) storedCoins_ += GetCheeseInfo(kind).coins;
    popupCount_ = carried_;
    popupPos_ = where;
    popupTimer_ = 1.2f;
    carried_ = 0;
    carriedWeight_ = 0;
    carriedKinds_.clear();
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

void Game::DrawBossHud() const
{
    if (bossIndex_ < 0) return;
    const Cat& boss = cats_[bossIndex_];
    const int w = LogicalWidth;
    // Name plate and three "health" paws that break as he crashes.
    const Rectangle plate{w / 2.0f - 150, static_cast<float>(LogicalHeight) - 37.0f, 300, 34}; // below the map, clear of the throne
    DrawRectangleRounded(plate, 0.4f, 8, Fade(BLACK, 0.6f));
    DrawText("SIR POUNCE", static_cast<int>(plate.x + 14), static_cast<int>(plate.y + 8), 20, Color{255, 150, 130, 255});
    for (int i = 0; i < Cat::BossHitsToWin; ++i)
    {
        const bool lost = i < boss.BossHits();
        const Vector2 c{plate.x + 190 + i * 36, plate.y + 17};
        const Color col = lost ? Color{80, 70, 70, 255} : Color{230, 70, 70, 255};
        DrawCircleV({c.x, c.y + 3}, 7, col); // paw pad
        for (int t = 0; t < 3; ++t) DrawCircleV({c.x - 7 + t * 7.0f, c.y - 6}, 3, col);
        if (lost) DrawLineEx({c.x - 9, c.y + 9}, {c.x + 9, c.y - 9}, 2, Color{200, 200, 200, 255});
    }
    if (bossFlash_ > 0.0f && !boss.IsKnockedOut())
    {
        const char* msg = TextFormat("CRASH!  %d / %d", boss.BossHits(), Cat::BossHitsToWin);
        const int size = 44;
        DrawText(msg, w / 2 - MeasureText(msg, size) / 2, 90, size, Fade(GOLD, std::min(1.0f, bossFlash_)));
    }
}

void Game::UpdateCats(float dt)
{
    // Cats: move, then check whether they can see or touch the mouse (never while it's hidden in a hole).
    for (size_t i = 0; i < cats_.size(); ++i)
    {
        Cat& cat = cats_[i];
        cat.Update(level_, dt);
        CatSounds(i, dt);

        catSeesPlayer_[i] = false;
        if (IsHidden()) continue;

        // Caught by contact. A lunging cat's paws reach a little further; a dazed or knocked-out cat is harmless.
        const float reach = cat.BodyRadius() + (cat.IsLunging() ? 6.0f : 0.0f);
        if (!cat.IsHarmless() && CheckCollisionCircles(player_.Position(), player_.Radius(), cat.Position(), reach))
        {
            state_ = State::Caught;
            Audio::Play(Sfx::Caught);
            Audio::DuckMusic(3.0f);
            return;
        }

        catSeesPlayer_[i] = cat.CanSee(level_, player_.Position(), player_.Radius());
        if (catSeesPlayer_[i]) cat.Alert(level_, player_.Position());
        CatSounds(i, 0.0f);
    }
}

void Game::CatSounds(size_t index, float dt)
{
    const Cat& cat = cats_[index];
    CatSoundState& was = catSound_[index];
    const Vector2 at = cat.Position();
    const Vector2 mouse = player_.Position();

    was.alertCooldown = std::max(0.0f, was.alertCooldown - dt);
    // (A cat that goes straight into its wind-up already has a sound of its own, so only a plain chase gets the alert.)
    if (cat.IsChasing() && !was.chasing && !cat.IsWindingUp() && !cat.IsLunging() && was.alertCooldown <= 0.0f)
    {
        Audio::PlayAt(Sfx::Alert, at, mouse);
        was.alertCooldown = 1.5f;
    }
    if (cat.IsWindingUp() && !was.windingUp) Audio::PlayAt(Sfx::WindUp, at, mouse);
    if (cat.IsLunging() && !was.lunging) Audio::PlayAt(Sfx::Pounce, at, mouse);

    if (cat.IsKnockedOut() && !was.knockedOut)
        Audio::PlayAt(Sfx::BossKo, at, mouse);
    else if (cat.IsStunned() && !was.stunned)
        Audio::PlayAt(cat.IsBoss() ? Sfx::BossHit : Sfx::Stun, at, mouse);

    was.chasing = cat.IsChasing();
    was.windingUp = cat.IsWindingUp();
    was.lunging = cat.IsLunging();
    was.stunned = cat.IsStunned();
    was.knockedOut = cat.IsKnockedOut();
}

void Game::Draw() const
{
    renderer_.Draw(level_, AllCheeseFound());

    for (const Cat& cat : cats_) cat.DrawRoute();

    const float now = static_cast<float>(GetTime());
    for (const CheesePiece& c : cheese_)
    {
        // Each piece sits at its own angle and gently bobs so it catches the eye.
        const float angle = std::fmod(c.pos.x * 0.37f + c.pos.y * 0.73f, 2.0f * PI);
        const float bob = std::sin(now * 2.5f + c.pos.x * 0.1f) * 1.5f;
        const float glow = 0.18f + 0.08f * std::sin(now * 3.0f + c.pos.y * 0.05f);
        DrawCircleV(c.pos, 19.0f, Fade(Color{255, 214, 90, 255}, glow)); // soft glow so pale cheeses stand out
        DrawCircleV(c.pos, 14.0f, Fade(Color{255, 230, 140, 255}, glow));
        DrawCheeseSprite(c.kind, {c.pos.x, c.pos.y + bob}, angle, 1.05f);
    }
    for (const Vector2& p : peppers_) DrawPepper(p, static_cast<float>(GetTime()));

    for (size_t i = 0; i < cats_.size(); ++i)
        cats_[i].Draw(level_, catSeesPlayer_[i], catVariant_[static_cast<int>(cats_[i].Kind())]);
    if (debug_)
        for (const Cat& cat : cats_) cat.DrawDebug();

    // Carried cheese trailing behind the mouse (farthest first so nearer ones draw on top).
    const float t = static_cast<float>(GetTime());
    for (int i = carried_ - 1; i >= 0; --i)
    {
        const float d = 30.0f + TrailSpacing * static_cast<float>(i);
        const Vector2 p = trail_.PointBehind(player_.Position(), d);
        // Point each piece along the trail, toward the mouse.
        const Vector2 ahead = trail_.PointBehind(player_.Position(), d - 6.0f);
        const float heading = (Vector2Distance(ahead, p) > 0.5f) ? std::atan2(ahead.y - p.y, ahead.x - p.x) : -PI / 2;
        const float wobble = std::sin(t * 8.0f + static_cast<float>(i)) * 0.15f;
        DrawCheeseSprite(carriedKinds_[i], p, heading + wobble, 0.85f);
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
        player_.Draw(IsFlashingRed(), skin_);
    }

    if (popupTimer_ > 0.0f)
    {
        const float rise = (1.2f - popupTimer_) * 30.0f;
        const char* msg = TextFormat("+%d stored", popupCount_);
        DrawText(msg, static_cast<int>(popupPos_.x) - MeasureText(msg, 20) / 2, static_cast<int>(popupPos_.y - 34 - rise), 20,
                 Fade(GOLD, std::min(1.0f, popupTimer_)));
    }

    // HUD
    const int w = LogicalWidth;
    const int h = LogicalHeight;
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
    else if (bossIndex_ >= 0 && !cats_[bossIndex_].IsKnockedOut())
    {
        hint = "Trick Sir Pounce into pouncing into walls or furniture!";
        hintColor = Color{255, 140, 120, 255};
    }
    else if (bossIndex_ >= 0 && !cheese_.empty())
    {
        hint = "He's out cold! Grab the Golden Cheese!";
        hintColor = GOLD;
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

    DrawBossHud();

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
        const char* msg = won ? (bossIndex_ >= 0 ? "SIR POUNCE DEFEATED!" : "LEVEL COMPLETE!") : "CAUGHT!";
        DrawText(msg, w / 2 - MeasureText(msg, 60) / 2, h / 2 - 80, 60, won ? GOLD : RED);

        if (won)
        {
            const char* time = TextFormat("Time %s%s", FormatTime(elapsed_).c_str(), newBest_ ? "   New best!" : "");
            DrawText(time, w / 2 - MeasureText(time, 28) / 2, h / 2 - 10, 28, newBest_ ? GREEN : RAYWHITE);
            if (!newBest_)
            {
                const char* best = TextFormat("Best %s", FormatTime(bestTime_).c_str());
                DrawText(best, w / 2 - MeasureText(best, 22) / 2, h / 2 + 24, 22, LIGHTGRAY);
            }
            // Coins earned (the shop's currency).
            const char* coins = TextFormat("+%d coins%s", CoinsEarned(), firstClear_ ? "  (first clear bonus!)" : "");
            const int coinsW = MeasureText(coins, 24);
            const float coinX = w / 2.0f - coinsW / 2.0f - 20;
            DrawCircleV({coinX, h / 2.0f + 62.0f}, 10, Color{190, 140, 20, 255});
            DrawCircleV({coinX, h / 2.0f + 62.0f}, 8, GOLD);
            DrawText(coins, w / 2 - coinsW / 2 + 4, h / 2 + 50, 24, GOLD);
        }
        else
        {
            const char* lost = TextFormat("You had %d of %d cheese stored", stored_, totalCheese_);
            DrawText(lost, w / 2 - MeasureText(lost, 24) / 2, h / 2 - 5, 24, LIGHTGRAY);
        }

        if (won && bossIndex_ >= 0)
        {
            const char* unlock = "World 2 unlocked!";
            DrawText(unlock, w / 2 - MeasureText(unlock, 30) / 2, h / 2 + 130, 30, Color{130, 225, 150, 255});
        }
        const char* sub = won ? (hasNextLevel_ ? (nextIsBoss_ ? "Enter: face the boss!     R: replay     Esc: menu" : "Enter: next level     R: replay     Esc: menu")
                                               : "R: replay     Esc: menu")
                              : "R: try again     Esc: menu";
        DrawText(sub, w / 2 - MeasureText(sub, 22) / 2, h / 2 + (won ? 92 : 70), 22, RAYWHITE);
    }
}
