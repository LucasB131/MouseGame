#pragma once

#include <string>
#include <vector>

#include "Level.h"
#include "Player.h"

class Game
{
public:
    bool Init(const std::string& levelPath);
    void Update(float dt);
    void Draw() const;

private:
    enum class State { Playing, Won };

    void Reset();
    bool ExitOpen() const { return cheese_.empty(); }

    static constexpr float CheeseRadius = 10.0f;

    Level level_;
    Player player_;
    std::vector<Vector2> cheese_; // cheese still on the map
    int totalCheese_ = 0;
    State state_ = State::Playing;
};
