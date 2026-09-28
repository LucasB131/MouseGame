// MouseGame - top-down stealth game (working title)

#include <string>

#include "Game.h"
#include "raylib.h"

int main()
{
    constexpr int screenWidth = 1280; // 32 tiles * 40 px
    constexpr int screenHeight = 720; // 18 tiles * 40 px

    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "MouseGame");
    SetTargetFPS(60);

    // Assets are copied next to the .exe by CMake, so load relative to the executable.
    const std::string levelPath = std::string(GetApplicationDirectory()) + "assets/levels/level1.txt";

    Game game;
    const bool loaded = game.Init(levelPath);

    while (!WindowShouldClose())
    {
        if (loaded) game.Update(GetFrameTime());

        BeginDrawing();
        ClearBackground(BLACK);
        if (loaded)
            game.Draw();
        else
            DrawText(TextFormat("Could not load level:\n%s", levelPath.c_str()), 20, 20, 20, RED);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
