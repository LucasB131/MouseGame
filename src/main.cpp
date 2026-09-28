// MouseGame - top-down stealth game (working title)
// Step 1 smoke test: a window, a movable mouse, and a piece of cheese to grab.

#include "raylib.h"
#include "raymath.h"

int main()
{
    constexpr int screenWidth  = 1280;
    constexpr int screenHeight = 720;

    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "MouseGame");
    SetTargetFPS(60);

    Vector2 mousePos   = {screenWidth / 2.0f, screenHeight / 2.0f};
    const float speed  = 250.0f;   // pixels per second
    const float radius = 18.0f;

    Vector2 cheesePos  = {200.0f, 150.0f};
    int cheeseCollected = 0;

    while (!WindowShouldClose())
    {
        // --- Update ---
        const float dt = GetFrameTime();
        Vector2 dir = {0.0f, 0.0f};
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    dir.y -= 1.0f;
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  dir.y += 1.0f;
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  dir.x -= 1.0f;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) dir.x += 1.0f;

        mousePos = Vector2Add(mousePos, Vector2Scale(Vector2Normalize(dir), speed * dt));
        mousePos.x = Clamp(mousePos.x, radius, screenWidth  - radius);
        mousePos.y = Clamp(mousePos.y, radius, screenHeight - radius);

        if (CheckCollisionCircles(mousePos, radius, cheesePos, 12.0f))
        {
            ++cheeseCollected;
            cheesePos = {static_cast<float>(GetRandomValue(40, screenWidth - 40)),
                         static_cast<float>(GetRandomValue(40, screenHeight - 40))};
        }

        // --- Draw ---
        BeginDrawing();
        ClearBackground({40, 44, 52, 255});

        DrawPoly(cheesePos, 3, 14.0f, 0.0f, GOLD);
        DrawCircleV(mousePos, radius, LIGHTGRAY);
        DrawCircleV(Vector2Add(mousePos, {-12, -14}), 8.0f, PINK);   // ears
        DrawCircleV(Vector2Add(mousePos, { 12, -14}), 8.0f, PINK);

        DrawText(TextFormat("Cheese: %d", cheeseCollected), 20, 20, 24, RAYWHITE);
        DrawText("WASD / Arrows to move, Esc to quit", 20, screenHeight - 34, 20, GRAY);
        DrawFPS(screenWidth - 100, 20);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
