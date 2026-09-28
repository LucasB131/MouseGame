// MouseGame - top-down stealth game (working title)

#include "App.h"
#include "raylib.h"

int main()
{
    constexpr int screenWidth = 1280; // 32 tiles * 40 px
    constexpr int screenHeight = 720; // 18 tiles * 40 px

    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "MouseGame");
    SetExitKey(KEY_NULL); // Esc is handled by the game (back to menu / quit from menu)
    SetTargetFPS(60);

    App app;
    app.Init();

    while (!WindowShouldClose() && app.Update())
    {
        BeginDrawing();
        ClearBackground(BLACK);
        app.Draw();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
