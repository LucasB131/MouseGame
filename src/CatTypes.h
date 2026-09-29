#pragma once

#include <string>

#include "raylib.h"

enum class CatKind { Tabby, Sleepy, Hunter, Blind, Boss };

constexpr int CatKindCount = 5;

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
    // Body size and pounce
    float radius;        // collision radius in pixels (normal cats 16)
    float lungeRange;    // starts a pounce when the mouse is this close and in sight
    float lungeDistance; // how far a pounce travels
    float lungeSpeed;    // pixels per second while pouncing
    float windUpTime;    // crouch before the pounce (the player's warning)
    float stunTime;      // seconds dazed after pouncing into a wall or furniture
    bool boss;           // takes hits instead of just getting dazed
};

const CatStats& GetCatStats(CatKind kind);

// Parses "tabby", "sleepy", "hunter", "blind" or "boss". Returns false if the text isn't a cat type.
bool ParseCatKind(const std::string& text, CatKind& out);
