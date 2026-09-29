#include "CatTypes.h"

namespace
{
const CatStats kStats[CatKindCount] = {
    // Tabby: the standard guard.
    {"Tabby", "Standard patrol guard", 90.0f, 160.0f, 200.0f, 0.62f, 0.0f, 0.0f,
     {225, 135, 50, 255}, {170, 90, 30, 255}, {180, 230, 60, 255},
     16.0f, 110.0f, 150.0f, 560.0f, 0.25f, 1.8f, false},

    // Sleepy: slow, short-sighted and naps regularly. Sneak past while it snoozes.
    {"Sleepy", "Naps often; sneak by while it snoozes", 55.0f, 120.0f, 160.0f, 0.85f, 3.0f, 3.5f,
     {150, 150, 170, 255}, {105, 105, 125, 255}, {240, 200, 80, 255},
     16.0f, 110.0f, 150.0f, 560.0f, 0.25f, 1.8f, false},

    // Hunter (Siamese): fast and sees far down a narrow cone.
    {"Hunter", "Fast, sees far down a narrow cone", 115.0f, 190.0f, 290.0f, 0.38f, 0.0f, 0.0f,
     {235, 220, 190, 255}, {85, 62, 50, 255}, {80, 160, 255, 255},
     16.0f, 110.0f, 150.0f, 560.0f, 0.25f, 1.8f, false},

    // Blind: senses all around it, but only up close. Normal speed.
    {"Blind", "Senses all around, but only up close", 90.0f, 160.0f, 110.0f, PI, 0.0f, 0.0f,
     {95, 90, 100, 255}, {60, 55, 65, 255}, {225, 228, 238, 255},
     16.0f, 110.0f, 150.0f, 560.0f, 0.25f, 1.8f, false},

    // Sir Pounce: World 1's boss. Huge, sees far, pounces from a distance. Only knocked out by
    // pouncing into walls or furniture three times.
    {"Sir Pounce", "The boss: trick him into crashing 3 times", 75.0f, 175.0f, 320.0f, 0.8f, 0.0f, 0.0f,
     {44, 42, 50, 255}, {24, 22, 28, 255}, {120, 230, 110, 255},
     24.0f, 175.0f, 270.0f, 640.0f, 0.9f, 2.6f, true},
};
} // namespace

const CatStats& GetCatStats(CatKind kind) { return kStats[static_cast<int>(kind)]; }

bool ParseCatKind(const std::string& text, CatKind& out)
{
    if (text == "tabby")  { out = CatKind::Tabby;  return true; }
    if (text == "sleepy") { out = CatKind::Sleepy; return true; }
    if (text == "hunter") { out = CatKind::Hunter; return true; }
    if (text == "blind")  { out = CatKind::Blind;  return true; }
    if (text == "boss")   { out = CatKind::Boss;   return true; }
    return false;
}
