#pragma once

#include <string>

#include "Skins.h"

// The player's wallet and wardrobe: coins earned from cheese, which skins they own and which is worn.
// Saved to a small text file next to the level save.
class Profile
{
public:
    void Load(const std::string& path);
    void Save() const;

    int Coins() const { return coins_; }
    void AddCoins(int amount);

    bool Owns(MouseSkin skin) const { return ((owned_ >> static_cast<int>(skin)) & 1u) != 0; }
    MouseSkin Equipped() const { return equipped_; }

    // Spends coins and adds the skin. Fails (returns false) if already owned or too expensive.
    bool Buy(MouseSkin skin);
    // Wears an owned skin.
    bool Equip(MouseSkin skin);

private:
    std::string path_;
    int coins_ = 0;
    unsigned owned_ = 1u; // bit per skin; the classic mouse is always owned
    MouseSkin equipped_ = MouseSkin::Classic;
};
