#pragma once

// Cosmetic skins for the mouse (more categories, like cat skins, can be added later).
enum class MouseSkin { Classic, Aviator };
constexpr int MouseSkinCount = 2;

struct SkinInfo
{
    const char* name;
    const char* line1; // short description, two lines for the shop card
    const char* line2;
    int price; // coins; 0 = free
};

const SkinInfo& GetSkinInfo(MouseSkin skin);
