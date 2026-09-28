#pragma once

#include <string>

#include "raylib.h"

enum class CatKind { Tabby, Sleepy, Hunter };

// Everything that makes one kind of cat play differently from another.
struct CatStats
{
    const char* name;
    const char* description;
    float patrolSpeed;      // pixels per second
    float chaseSpeed;       // pixels per second while investigating (player runs at 220)
    float viewRange;        // pixels
    float viewHalfAngle;    // radians, each side of where it's facing
    float detectMultiplier; // how fast it fills the detection meter (1 = normal)
    float awakeTime;        // seconds awake between naps
    float sleepTime;        // seconds asleep per nap (0 = never sleeps)
    Color fur;
    Color furDark;
    Color eye;
};

const CatStats& GetCatStats(CatKind kind);

// Parses "tabby", "sleepy" or "hunter". Returns false if the text isn't a cat type.
bool ParseCatKind(const std::string& text, CatKind& out);

constexpr int CatKindCount = 3;
