#include "Viewport.h"

#include <algorithm>
#include <cmath>

#include "rlgl.h"

namespace
{
float gScale = 1.0f;
float gOffsetX = 0.0f;
float gOffsetY = 0.0f;
} // namespace

void HandleFullscreenKeys()
{
    const bool altEnter = (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && IsKeyPressed(KEY_ENTER);
    if (IsKeyPressed(KEY_F11) || altEnter) ToggleBorderlessWindowed();
}

void UpdateViewport()
{
    const float w = static_cast<float>(GetScreenWidth());
    const float h = static_cast<float>(GetScreenHeight());
    gScale = std::min(w / LogicalWidth, h / LogicalHeight);
    gOffsetX = std::floor((w - LogicalWidth * gScale) / 2.0f);
    gOffsetY = std::floor((h - LogicalHeight * gScale) / 2.0f);

    // Mouse queries (GetMousePosition and friends) now come back in logical coordinates.
    SetMouseOffset(static_cast<int>(-gOffsetX), static_cast<int>(-gOffsetY));
    SetMouseScale(1.0f / gScale, 1.0f / gScale);
}

void BeginLogicalView()
{
    rlPushMatrix();
    rlTranslatef(gOffsetX, gOffsetY, 0.0f);
    rlScalef(gScale, gScale, 1.0f);
}

void EndLogicalView()
{
    rlDrawRenderBatchActive();
    rlPopMatrix();

    // Black bars over anything that spilled outside the logical screen.
    const int w = GetScreenWidth();
    const int h = GetScreenHeight();
    const int x0 = static_cast<int>(gOffsetX);
    const int y0 = static_cast<int>(gOffsetY);
    const int x1 = static_cast<int>(std::ceil(gOffsetX + LogicalWidth * gScale));
    const int y1 = static_cast<int>(std::ceil(gOffsetY + LogicalHeight * gScale));
    if (x0 > 0) { DrawRectangle(0, 0, x0, h, BLACK); DrawRectangle(x1, 0, w - x1, h, BLACK); }
    if (y0 > 0) { DrawRectangle(0, 0, w, y0, BLACK); DrawRectangle(0, y1, w, h - y1, BLACK); }
}

void BeginLogicalScissor(Rectangle r)
{
    BeginScissorMode(static_cast<int>(std::floor(gOffsetX + r.x * gScale)), static_cast<int>(std::floor(gOffsetY + r.y * gScale)),
                     static_cast<int>(std::ceil(r.width * gScale)), static_cast<int>(std::ceil(r.height * gScale)));
}
