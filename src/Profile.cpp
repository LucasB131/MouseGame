#include "Profile.h"

#include <algorithm>
#include <fstream>

void Profile::Load(const std::string& path)
{
    path_ = path;
    coins_ = 0;
    owned_ = 1u;
    equipped_ = MouseSkin::Classic;

    // Format: "coins N", "owned <bitmask>", "equipped <skin index>"
    std::ifstream in(path);
    std::string key;
    int value = 0;
    while (in >> key >> value)
    {
        if (key == "coins") coins_ = std::max(0, value);
        else if (key == "owned") owned_ = static_cast<unsigned>(value) & ((1u << MouseSkinCount) - 1u);
        else if (key == "equipped" && value >= 0 && value < MouseSkinCount) equipped_ = static_cast<MouseSkin>(value);
    }
    owned_ |= 1u;
    if (!Owns(equipped_)) equipped_ = MouseSkin::Classic;
}

void Profile::Save() const
{
    if (path_.empty()) return;
    std::ofstream out(path_);
    out << "coins " << coins_ << '\n' << "owned " << owned_ << '\n' << "equipped " << static_cast<int>(equipped_) << '\n';
}

void Profile::AddCoins(int amount)
{
    coins_ = std::max(0, coins_ + amount);
}

bool Profile::Buy(MouseSkin skin)
{
    const int price = GetSkinInfo(skin).price;
    if (Owns(skin) || coins_ < price) return false;
    coins_ -= price;
    owned_ |= 1u << static_cast<int>(skin);
    return true;
}

bool Profile::Equip(MouseSkin skin)
{
    if (!Owns(skin)) return false;
    equipped_ = skin;
    return true;
}
