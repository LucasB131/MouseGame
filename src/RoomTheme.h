#pragma once

#include <string>

#include "raylib.h"

// What kind of room a level is. Sets the floor, the walls and how furniture is decorated.
enum class RoomTheme
{
    Foyer, Living, Dining, Hallway, Kitchen, Pantry, Library, Billiards, Study, Music,
    Conservatory, Laundry, Bathroom, Bedroom, Nursery, GameRoom, Theater, Cellar, Garage,
    Basement, Attic, Ballroom, Gallery, Parlor,
    // World 2 - Egypt wing (keep these last: IsEgypt() relies on the order)
    Pyramid, GreatGallery, ColumnHall, Tomb, Vault
};

enum class FloorStyle { Wood, Checker, Tile, Carpet, Concrete, Marble, Flagstone, Basalt };

struct ThemeStyle
{
    FloorStyle floor;
    Color floorA;     // main floor color
    Color floorB;     // secondary floor color (planks / checker / grout)
    Color wall;       // wall face
    Color wallTop;    // wall highlight
    Color accent;     // sofas, beds, cushions
    Color wood;       // tables, shelves, frames
    bool stone = false; // carved stone walls with a hieroglyph frieze (Egypt rooms)
};

inline bool IsEgyptTheme(RoomTheme t) { return t >= RoomTheme::Pyramid; }

const ThemeStyle& GetThemeStyle(RoomTheme theme);
bool ParseRoomTheme(const std::string& text, RoomTheme& out);
