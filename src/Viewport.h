#pragma once

#include "raylib.h"

// The game is laid out for a fixed 1280x720 "logical" screen (32x18 tiles of 40 px). The window can be any size
// (resizable, or borderless fullscreen with F11 / Alt+Enter): everything is scaled to fit and centered with black
// bars, and the mouse position is mapped back into logical coordinates.
constexpr int LogicalWidth = 1280;
constexpr int LogicalHeight = 720;

// F11 or Alt+Enter toggles borderless fullscreen. Call once per frame, before drawing.
void HandleFullscreenKeys();

// Once per frame, before updating the game: works out the scale/centering and maps the mouse into logical coordinates.
void UpdateViewport();

// Wrap all game drawing (after BeginDrawing, before EndDrawing) in these two calls.
void BeginLogicalView();
void EndLogicalView();

// Like BeginScissorMode, but the rectangle is in logical coordinates.
void BeginLogicalScissor(Rectangle logicalRect);
