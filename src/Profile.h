#pragma once

#include <cstdint>
#include <string>

#include "Skins.h"

// The player's wallet and wardrobe: coins earned from cheese, which skins they own and which one is worn in
// each category (mouse, and each cat type). Saved to a small text file next to the level saves.
class Profile
{
public:
    void Load(const std::string& path);
    void Save() const;

    int Coins() const { return coins_; }
    void AddCoins(int amount);

    bool Owns(int skinId) const { return ((owned_ >> skinId) & 1u) != 0; }
    int Equipped(SkinCategory category) const { return equipped_[static_cast<int>(category)]; }
    bool IsEquipped(int skinId) const { return Equipped(GetSkinInfo(skinId).category) == skinId; }

    // The variant to draw for the mouse / for cats of a type.
    MouseSkin MouseLook() const { return static_cast<MouseSkin>(GetSkinInfo(Equipped(SkinCategory::Mouse)).variant); }
    int CatVariant(CatKind kind) const { return kind == CatKind::Boss ? 0 : GetSkinInfo(Equipped(CategoryForKind(kind))).variant; }

    // Spends coins and adds the skin. Fails (returns false) if already owned or too expensive.
    bool Buy(int skinId);
    // Wears an owned skin (replacing whatever is worn in its category).
    bool Equip(int skinId);

private:
    void Reset();

    std::string path_;
    int coins_ = 0;
    std::uint64_t owned_ = 0;
    int equipped_[SkinCategoryCount] = {};
};
