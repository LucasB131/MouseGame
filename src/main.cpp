// Sneak - top-down stealth game

#include "App.h"
#include "Viewport.h"
#include "raylib.h"

int main()
{
    // The window starts at the logical size (32 x 18 tiles of 40 px) but can be resized, and F11 goes fullscreen.
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(LogicalWidth, LogicalHeight, "Sneak");
    SetWindowMinSize(LogicalWidth / 2, LogicalHeight / 2);
    SetExitKey(KEY_NULL); // Esc is handled by the game (back to menu / quit from menu)
    SetTargetFPS(60);

    App app;
    app.Init();

    while (!WindowShouldClose())
    {
        HandleFullscreenKeys();
        UpdateViewport(); // mouse mapping must be set before the game reads the mouse
        const bool keepGoing = app.Update();
        BeginDrawing();
        ClearBackground(BLACK);
        BeginLogicalView();
        app.Draw();
        EndLogicalView();
        EndDrawing();
        if (!keepGoing) break;
    }

    CloseWindow();
    return 0;
}
