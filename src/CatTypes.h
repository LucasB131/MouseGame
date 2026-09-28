#pragma once

#include <string>

#include "raylib.h"

enum class CatKind { Tabby, Sleepy, Hunter, Blind };

constexpr int CatKindCount = 4;

// Everything that makes one kind of cat play differently from another.
struct CatStats
{
    const char* name;
    const char* description;
    float patrolSpeed;   // pixels per second
    float chaseSpeed;    // pixels per second while chasing (the mouse runs at 220, slower with cheese)
    float viewRange;     // pixels
    float viewHalfAngle; // radians each side of facing; PI = senses in a full circle
    float awakeTime;     // seconds awake between naps
    float sleepTime;     // seconds asleep per nap (0 = never sleeps)
    Color fur;
    Color furDark;
    Color eye;
};

const CatStats& GetCatStats(CatKind kind);

// Parses "tabby", "sleepy", "hunter" or "blind". Returns false if the text isn't a cat type.
bool ParseCatKind(const std::string& text, CatKind& out);
