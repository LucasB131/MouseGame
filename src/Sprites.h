#pragma once

#include "CatTypes.h"
#include "Cheese.h"
#include "Skins.h"
#include "raylib.h"

// Hand-drawn (code-drawn) character art, shared by the game and the menus.
// Everything is drawn top-down around a position, facing a unit direction, at an optional scale.

struct MouseLook
{
    Vector2 pos{};
    Vector2 facing{1.0f, 0.0f};
    float walkPhase = 0.0f; // advances with distance walked; drives the feet
    bool moving = false;
    bool flashRed = false; // pepper boost
    float scale = 1.0f;
    MouseSkin skin = MouseSkin::Classic;
};
void DrawMouseSprite(const MouseLook& m);

enum class CatPose { Normal, Asleep, Stunned, WindUp, Lunge };

struct CatLook
{
    Vector2 pos{};
    Vector2 facing{1.0f, 0.0f};
    CatKind kind = CatKind::Tabby;
    int variant = 0;     // costume/coat from the shop (0 = default); see Skins.cpp
    CatPose pose = CatPose::Normal;
    float walkPhase = 0.0f;
    bool moving = false;
    bool alert = false;  // chasing: wide pupils, faster tail
    float tailSeed = 0;  // so cats' tails don't swish in sync
    float scale = 1.0f;
};
void DrawCatSprite(const CatLook& c);

void DrawCheeseSprite(CheeseKind kind, Vector2 pos, float rotation, float scale = 1.0f);
