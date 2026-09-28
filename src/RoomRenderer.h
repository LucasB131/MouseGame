#pragma once

#include "Level.h"
#include "raylib.h"

// Draws a level as a themed room: floor, rugs, walls, shadows and furniture art.
// Everything that never changes is rendered once into a texture, so each frame is a single blit.
class RoomRenderer
{
public:
    RoomRenderer() = default;
    ~RoomRenderer();
    RoomRenderer(const RoomRenderer&) = delete;
    RoomRenderer& operator=(const RoomRenderer&) = delete;

    void Build(const Level& level); // call after loading a level (needs an open window)
    void Draw(const Level& level, bool exitOpen) const;

private:
    void Release();

    RenderTexture2D target_{};
    bool built_ = false;
};
