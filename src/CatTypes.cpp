#include "CatTypes.h"

namespace
{
const CatStats kStats[CatKindCount] = {
    // Tabby: the standard guard.
    {"Tabby", "Standard patrol guard", 90.0f, 145.0f, 200.0f, 0.62f, 1.0f, 0.0f, 0.0f,
     {225, 135, 50, 255}, {170, 90, 30, 255}, {180, 230, 60, 255}},

    // Sleepy: slow, short-sighted and naps regularly. Sneak past while it snoozes.
    {"Sleepy", "Naps often; sneak by while it snoozes", 55.0f, 110.0f, 160.0f, 0.85f, 0.8f, 3.0f, 3.5f,
     {150, 150, 170, 255}, {105, 105, 125, 255}, {240, 200, 80, 255}},

    // Hunter (Siamese): fast, sees far down a narrow cone, and recognizes you quickly.
    {"Hunter", "Fast, sees far, spots you quickly", 115.0f, 175.0f, 290.0f, 0.38f, 1.5f, 0.0f, 0.0f,
     {235, 220, 190, 255}, {85, 62, 50, 255}, {80, 160, 255, 255}},
};
} // namespace

const CatStats& GetCatStats(CatKind kind) { return kStats[static_cast<int>(kind)]; }

bool ParseCatKind(const std::string& text, CatKind& out)
{
    if (text == "tabby")  { out = CatKind::Tabby;  return true; }
    if (text == "sleepy") { out = CatKind::Sleepy; return true; }
    if (text == "hunter") { out = CatKind::Hunter; return true; }
    return false;
}
