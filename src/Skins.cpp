#include "Skins.h"

namespace
{
const SkinInfo kSkins[MouseSkinCount] = {
    {"Classic Mouse", "The little grey", "hero, as always.", 0},
    {"Ace Aviator", "Leather cap, goggles", "and a windswept scarf.", 60},
};
} // namespace

const SkinInfo& GetSkinInfo(MouseSkin skin) { return kSkins[static_cast<int>(skin)]; }
