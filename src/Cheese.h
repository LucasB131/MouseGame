#pragma once

// The kinds of cheese. Light cheeses ('C' in a level file) get a variety picked from their position;
// a Gouda wheel ('O') is a heavy prize that slows the mouse down twice as much.
// The Golden Cheese is dropped by a knocked-out boss.
enum class CheeseKind { Cheddar, Swiss, Brie, Blue, Gouda, Golden };

struct CheeseInfo
{
    const char* name;
    int weight; // how much it slows the mouse while carried (1 = normal piece)
    int coins;  // what it is worth in the shop currency when you get it home
};

const CheeseInfo& GetCheeseInfo(CheeseKind kind);

// Deterministic variety for plain 'C' cheese at tile (x, y).
CheeseKind LightCheeseAt(int x, int y);
