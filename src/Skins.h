#pragma once

#include "CatTypes.h"

// Cosmetic skins. Every skin belongs to one category: the mouse, or one of the four regular cat types
// (an equipped cat skin dresses every cat of that type). Skins have a global id, mice first.
enum class SkinCategory { Mouse, Tabby, Sleepy, Hunter, Blind };
constexpr int SkinCategoryCount = 5;

// The mouse's looks (the variant index of a Mouse skin).
enum class MouseSkin { Classic, Aviator, Pirate, Chef, Wizard, Ninja, Golden };

struct SkinInfo
{
    SkinCategory category;
    int variant; // index within the category; 0 is the free default. For cats, drawn by DrawCatSprite
    const char* name;
    const char* line1; // short description, two lines for the shop card
    const char* line2;
    int price; // coins; 0 = free
};

constexpr int SkinCount = 19;
const SkinInfo& GetSkinInfo(int id);

const char* SkinCategoryName(SkinCategory category);
int SkinsInCategory(SkinCategory category);
int SkinIdIn(SkinCategory category, int index); // global id of the index-th skin of a category
int DefaultSkinId(SkinCategory category);
SkinCategory CategoryForKind(CatKind kind);            // only call with Tabby..Blind
