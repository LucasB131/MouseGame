#include "Profile.h"

#include <algorithm>
#include <fstream>

void Profile::Reset()
{
    coins_ = 0;
    owned_ = 0;
    for (int c = 0; c < SkinCategoryCount; ++c)
    {
        equipped_[c] = DefaultSkinId(static_cast<SkinCategory>(c));
        owned_ |= 1ull << equipped_[c]; // the default of every category is always owned
    }
}

void Profile::Load(const std::string& path)
{
    path_ = path;
    Reset();

    // Format: "coins N", "owned <bitmask>", "equip0".."equip4 <skin id>" (older files: "equipped <mouse skin id>").
    std::ifstream in(path);
    std::string key;
    long long value = 0;
    while (in >> key >> value)
    {
        if (key == "coins") coins_ = static_cast<int>(std::max(0ll, value));
        else if (key == "owned") owned_ |= static_cast<std::uint64_t>(value) & ((1ull << SkinCount) - 1ull);
        else if (key == "equipped") equipped_[0] = static_cast<int>(value); // legacy: mouse skin
        else if (key.size() == 6 && key.compare(0, 5, "equip") == 0 && key[5] >= '0' && key[5] < '0' + SkinCategoryCount)
            equipped_[key[5] - '0'] = static_cast<int>(value);
    }
    // Never wear something that isn't owned or belongs to another category.
    for (int c = 0; c < SkinCategoryCount; ++c)
    {
        const int id = equipped_[c];
        if (id < 0 || id >= SkinCount || !Owns(id) || GetSkinInfo(id).category != static_cast<SkinCategory>(c))
            equipped_[c] = DefaultSkinId(static_cast<SkinCategory>(c));
    }
}

void Profile::Save() const
{
    if (path_.empty()) return;
    std::ofstream out(path_);
    out << "coins " << coins_ << '\n' << "owned " << owned_ << '\n';
    for (int c = 0; c < SkinCategoryCount; ++c) out << "equip" << c << ' ' << equipped_[c] << '\n';
}

void Profile::AddCoins(int amount)
{
    coins_ = std::max(0, coins_ + amount);
}

bool Profile::Buy(int skinId)
{
    if (skinId < 0 || skinId >= SkinCount) return false;
    const int price = GetSkinInfo(skinId).price;
    if (Owns(skinId) || coins_ < price) return false;
    coins_ -= price;
    owned_ |= 1ull << skinId;
    return true;
}

bool Profile::Equip(int skinId)
{
    if (skinId < 0 || skinId >= SkinCount || !Owns(skinId)) return false;
    equipped_[static_cast<int>(GetSkinInfo(skinId).category)] = skinId;
    return true;
}
