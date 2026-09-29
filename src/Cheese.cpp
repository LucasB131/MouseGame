#include "Cheese.h"

namespace
{
const CheeseInfo kInfo[] = {
    {"Cheddar", 1, 1},
    {"Swiss", 1, 1},
    {"Brie", 1, 1},
    {"Blue", 1, 1},
    {"Gouda", 2, 2},
    {"Golden Cheese", 1, 25},
};
} // namespace

const CheeseInfo& GetCheeseInfo(CheeseKind kind) { return kInfo[static_cast<int>(kind)]; }

CheeseKind LightCheeseAt(int x, int y)
{
    unsigned h = static_cast<unsigned>(x) * 73856093u ^ static_cast<unsigned>(y) * 19349663u;
    h ^= h >> 7;
    return static_cast<CheeseKind>(h % 4); // Cheddar, Swiss, Brie or Blue
}
