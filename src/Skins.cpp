#include "Skins.h"

namespace
{
const SkinInfo kSkins[SkinCount] = {
    // Mouse
    {SkinCategory::Mouse, 0, "Classic Mouse", "The little grey", "hero, as always.", 0},
    {SkinCategory::Mouse, 1, "Ace Aviator", "Leather cap, goggles", "and a windswept scarf.", 60},
    {SkinCategory::Mouse, 2, "Buccaneer", "Bandana, eyepatch", "and a gold hoop.", 80},
    {SkinCategory::Mouse, 3, "Chef Mouse", "A tall white toque", "and a red neckerchief.", 80},
    {SkinCategory::Mouse, 4, "Wee Wizard", "A starry purple hat", "and a lavender coat.", 100},
    {SkinCategory::Mouse, 5, "Shadow Ninja", "Dark as night, with", "a red headband.", 120},
    {SkinCategory::Mouse, 6, "Golden Mouse", "Pure gold and", "twinkling all over.", 250},
    // Tabby
    {SkinCategory::Tabby, 0, "Ginger Tabby", "The standard orange", "striped guard.", 0},
    {SkinCategory::Tabby, 1, "Midnight", "A sleek black cat", "with a blue collar.", 70},
    {SkinCategory::Tabby, 2, "Dapper Tabby", "A black top hat and", "a very serious look.", 90},
    // Sleepy
    {SkinCategory::Sleepy, 0, "Grey Fluff", "The dozy, fluffy", "grey napper.", 0},
    {SkinCategory::Sleepy, 1, "Cream Puff", "A cloud of cream", "and white fluff.", 70},
    {SkinCategory::Sleepy, 2, "Nightcap", "Striped nightcap with", "a bobbing pompom.", 90},
    // Hunter
    {SkinCategory::Hunter, 0, "Siamese", "The fast, sharp-eyed", "dark-pointed hunter.", 0},
    {SkinCategory::Hunter, 1, "Chocolate Point", "Rich brown points", "on a warm fawn coat.", 70},
    {SkinCategory::Hunter, 2, "Ninja Cat", "A navy headband with", "streaming tails.", 100},
    // Blind
    {SkinCategory::Blind, 0, "Misty Grey", "The cloudy-eyed cat", "who hears everything.", 0},
    {SkinCategory::Blind, 1, "Snowy", "A pure white coat", "and pale blue eyes.", 70},
    {SkinCategory::Blind, 2, "Cool Shades", "Sunglasses, because", "why not.", 90},
};
} // namespace

const SkinInfo& GetSkinInfo(int id) { return kSkins[id]; }

const char* SkinCategoryName(SkinCategory category)
{
    static const char* const names[SkinCategoryCount] = {"Mouse", "Tabby", "Sleepy", "Hunter", "Blind"};
    return names[static_cast<int>(category)];
}

int SkinsInCategory(SkinCategory category)
{
    int n = 0;
    for (const SkinInfo& s : kSkins) n += s.category == category;
    return n;
}

int SkinIdIn(SkinCategory category, int index)
{
    int seen = 0;
    for (int id = 0; id < SkinCount; ++id)
        if (kSkins[id].category == category && seen++ == index) return id;
    return DefaultSkinId(category);
}

int DefaultSkinId(SkinCategory category)
{
    for (int id = 0; id < SkinCount; ++id)
        if (kSkins[id].category == category) return id;
    return 0;
}

SkinCategory CategoryForKind(CatKind kind)
{
    switch (kind)
    {
    case CatKind::Sleepy: return SkinCategory::Sleepy;
    case CatKind::Hunter: return SkinCategory::Hunter;
    case CatKind::Blind:  return SkinCategory::Blind;
    default:              return SkinCategory::Tabby;
    }
}
