#include "RoomRenderer.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
constexpr int TS = Level::TileSize;

// ------------------------------------------------------------------ small helpers

unsigned Hash(int x, int y, int salt = 0)
{
    unsigned h = static_cast<unsigned>(x) * 374761393u + static_cast<unsigned>(y) * 668265263u + static_cast<unsigned>(salt) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

float Rand01(int x, int y, int salt = 0) { return static_cast<float>(Hash(x, y, salt) % 10000) / 10000.0f; }

Color Shade(Color c, float f)
{
    auto ch = [f](unsigned char v) { return static_cast<unsigned char>(std::clamp(v * f, 0.0f, 255.0f)); };
    return {ch(c.r), ch(c.g), ch(c.b), c.a};
}

Color Mix(Color a, Color b, float t)
{
    auto ch = [t](unsigned char x, unsigned char y) { return static_cast<unsigned char>(x + (y - x) * t); };
    return {ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b), 255};
}

Rectangle Inset(Rectangle r, float d) { return {r.x + d, r.y + d, r.width - 2 * d, r.height - 2 * d}; }

void Box(Rectangle r, Color fill, float round = 0.15f)
{
    DrawRectangleRounded(r, round, 6, fill);
}

void BoxOutlined(Rectangle r, Color fill, Color edge, float round = 0.15f, float thick = 2.0f)
{
    DrawRectangleRounded(r, round, 6, fill);
    DrawRectangleRoundedLinesEx(r, round, 6, thick, edge);
}

// A furniture piece: a rectangle of identical furniture letters, in tiles and pixels.
struct Piece
{
    char kind;
    int tx, ty, tw, th;
    Rectangle r;
    int index; // how many pieces of this kind came before it (for variety)
};

enum class Side { Top, Bottom, Left, Right };

bool Horizontal(const Piece& p) { return p.tw >= p.th; }

// Is every tile just outside the given side a wall?
bool TouchesWall(const Level& level, const Piece& p, Side side)
{
    auto isWallTile = [&](int x, int y) { return level.At(x, y) == Tile::Wall; };
    switch (side)
    {
    case Side::Top:    for (int x = p.tx; x < p.tx + p.tw; ++x) if (!isWallTile(x, p.ty - 1)) return false; return true;
    case Side::Bottom: for (int x = p.tx; x < p.tx + p.tw; ++x) if (!isWallTile(x, p.ty + p.th)) return false; return true;
    case Side::Left:   for (int y = p.ty; y < p.ty + p.th; ++y) if (!isWallTile(p.tx - 1, y)) return false; return true;
    case Side::Right:  for (int y = p.ty; y < p.ty + p.th; ++y) if (!isWallTile(p.tx + p.tw, y)) return false; return true;
    }
    return false;
}

// Seats face whatever they're gathered around (a table, fireplace, TV, fountain...).
// Returns the side the backrest goes on.
Side SeatBackSide(const Level& level, const Piece& p, RoomTheme theme)
{
    auto isFocus = [&](int x, int y) {
        const char c = level.FurnitureAt(x, y);
        return c == 'T' || c == 'F' || c == 'K' || c == 'G' || c == 'U' || c == 'Q';
    };
    int best = 99;
    Side facing = Side::Top;
    for (int d = 1; d <= 4; ++d)
    {
        for (int x = p.tx; x < p.tx + p.tw; ++x)
        {
            if (d < best && isFocus(x, p.ty - d)) { best = d; facing = Side::Top; }
            if (d < best && isFocus(x, p.ty + p.th - 1 + d)) { best = d; facing = Side::Bottom; }
        }
        for (int y = p.ty; y < p.ty + p.th; ++y)
        {
            if (d < best && isFocus(p.tx - d, y)) { best = d; facing = Side::Left; }
            if (d < best && isFocus(p.tx + p.tw - 1 + d, y)) { best = d; facing = Side::Right; }
        }
    }
    if (best < 99)
    {
        switch (facing)
        {
        case Side::Top: return Side::Bottom;
        case Side::Bottom: return Side::Top;
        case Side::Left: return Side::Right;
        case Side::Right: return Side::Left;
        }
    }
    for (Side s : {Side::Top, Side::Left, Side::Right, Side::Bottom})
        if (TouchesWall(level, p, s)) return s;
    if (theme == RoomTheme::Theater) return Side::Bottom;
    return Horizontal(p) ? Side::Top : Side::Left;
}

Side HeadSide(const Level& level, const Piece& p)
{
    for (Side s : {Side::Top, Side::Left, Side::Right, Side::Bottom})
        if (TouchesWall(level, p, s)) return s;
    return Side::Top;
}

// Strip of the rectangle along one side.
Rectangle Strip(Rectangle r, Side side, float thickness)
{
    switch (side)
    {
    case Side::Top: return {r.x, r.y, r.width, thickness};
    case Side::Bottom: return {r.x, r.y + r.height - thickness, r.width, thickness};
    case Side::Left: return {r.x, r.y, thickness, r.height};
    case Side::Right: return {r.x + r.width - thickness, r.y, thickness, r.height};
    }
    return r;
}

// ------------------------------------------------------------------ floors

void DrawFloorTile(const ThemeStyle& st, int x, int y)
{
    const float px = static_cast<float>(x * TS);
    const float py = static_cast<float>(y * TS);
    switch (st.floor)
    {
    case FloorStyle::Wood:
    {
        // Two planks per tile with staggered seams.
        for (int k = 0; k < 2; ++k)
        {
            const float t = Rand01(x, y * 2 + k, 1);
            const Color c = Mix(st.floorA, st.floorB, t * 0.8f);
            DrawRectangle(static_cast<int>(px), static_cast<int>(py + k * 20), TS, 20, c);
            DrawLine(static_cast<int>(px), static_cast<int>(py + k * 20 + 19), static_cast<int>(px + TS), static_cast<int>(py + k * 20 + 19), Shade(st.floorB, 0.8f));
            if (Hash(x, y * 2 + k, 7) % 3 == 0)
            {
                const int seam = static_cast<int>(px) + 6 + static_cast<int>(Hash(x, y * 2 + k, 9) % 28);
                DrawLine(seam, static_cast<int>(py + k * 20), seam, static_cast<int>(py + k * 20 + 19), Shade(st.floorB, 0.8f));
            }
        }
        break;
    }
    case FloorStyle::Checker:
        DrawRectangle(static_cast<int>(px), static_cast<int>(py), TS, TS, ((x + y) % 2 == 0) ? st.floorA : st.floorB);
        DrawRectangleLines(static_cast<int>(px), static_cast<int>(py), TS, TS, Fade(BLACK, 0.12f));
        break;
    case FloorStyle::Tile:
        DrawRectangle(static_cast<int>(px), static_cast<int>(py), TS, TS, st.floorA);
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < 2; ++j)
            {
                const Color c = Mix(st.floorA, WHITE, Rand01(x * 2 + i, y * 2 + j, 3) * 0.08f);
                DrawRectangle(static_cast<int>(px) + i * 20 + 1, static_cast<int>(py) + j * 20 + 1, 18, 18, c);
            }
        break;
    case FloorStyle::Carpet:
        DrawRectangle(static_cast<int>(px), static_cast<int>(py), TS, TS, st.floorA);
        for (int i = 0; i < 10; ++i)
        {
            const int dx = static_cast<int>(Hash(x, y, 20 + i) % TS);
            const int dy = static_cast<int>(Hash(x, y, 40 + i) % TS);
            DrawRectangle(static_cast<int>(px) + dx, static_cast<int>(py) + dy, 2, 2, st.floorB);
        }
        break;
    case FloorStyle::Concrete:
        DrawRectangle(static_cast<int>(px), static_cast<int>(py), TS, TS, Mix(st.floorA, st.floorB, Rand01(x / 3, y / 3, 5) * 0.5f));
        for (int i = 0; i < 6; ++i)
        {
            const int dx = static_cast<int>(Hash(x, y, 60 + i) % TS);
            const int dy = static_cast<int>(Hash(x, y, 80 + i) % TS);
            DrawRectangle(static_cast<int>(px) + dx, static_cast<int>(py) + dy, 2, 2, Shade(st.floorB, 0.85f));
        }
        if (Hash(x, y, 99) % 17 == 0)
            DrawLineEx({px + 5, py + 8}, {px + 30, py + 26}, 1.0f, Shade(st.floorB, 0.75f)); // crack
        break;
    case FloorStyle::Flagstone:
    {
        // Two staggered courses of sandstone blocks per tile.
        for (int k = 0; k < 2; ++k)
        {
            const int y0 = static_cast<int>(py) + k * 20;
            const float t = Rand01(x, y * 2 + k, 15);
            DrawRectangle(static_cast<int>(px), y0, TS, 20, Mix(st.floorA, st.floorB, t * 0.7f));
            DrawLine(static_cast<int>(px), y0 + 19, static_cast<int>(px + TS), y0 + 19, Shade(st.floorB, 0.78f));
            const int seam = static_cast<int>(px) + (((x + y * 2 + k) % 2) ? 6 : 26);
            DrawLine(seam, y0, seam, y0 + 19, Shade(st.floorB, 0.78f));
            if (Hash(x, y * 2 + k, 16) % 5 == 0)
                DrawRectangle(static_cast<int>(px) + 4 + static_cast<int>(Hash(x, y, 17) % 30), y0 + 4 + static_cast<int>(Hash(x, y, 18) % 10), 2, 2, Shade(st.floorB, 0.85f));
        }
        break;
    }
    case FloorStyle::Basalt:
    {
        // Big dark slabs with a faint sheen and pale flecks.
        const bool alt = ((x / 2 + y / 2) % 2) == 0;
        DrawRectangle(static_cast<int>(px), static_cast<int>(py), TS, TS, alt ? st.floorA : st.floorB);
        if (x % 2 == 0) DrawLine(static_cast<int>(px), static_cast<int>(py), static_cast<int>(px), static_cast<int>(py + TS), Fade(BLACK, 0.25f));
        if (y % 2 == 0) DrawLine(static_cast<int>(px), static_cast<int>(py), static_cast<int>(px + TS), static_cast<int>(py), Fade(BLACK, 0.25f));
        for (int i = 0; i < 4; ++i)
            DrawRectangle(static_cast<int>(px) + static_cast<int>(Hash(x, y, 120 + i) % TS), static_cast<int>(py) + static_cast<int>(Hash(x, y, 130 + i) % TS), 2, 2, Fade(WHITE, 0.12f));
        break;
    }
    case FloorStyle::Marble:
    {
        // Big 2x2-tile slabs with faint veins.
        const bool alt = ((x / 2 + y / 2) % 2) == 0;
        DrawRectangle(static_cast<int>(px), static_cast<int>(py), TS, TS, alt ? st.floorA : st.floorB);
        if (x % 2 == 0) DrawLine(static_cast<int>(px), static_cast<int>(py), static_cast<int>(px), static_cast<int>(py + TS), Fade(BLACK, 0.15f));
        if (y % 2 == 0) DrawLine(static_cast<int>(px), static_cast<int>(py), static_cast<int>(px + TS), static_cast<int>(py), Fade(BLACK, 0.15f));
        if (Hash(x, y, 11) % 3 == 0)
        {
            const float a = Rand01(x, y, 12) * 30.0f;
            DrawLineEx({px + 2, py + 10 + a / 3}, {px + 38, py + a}, 1.0f, Fade(WHITE, 0.12f));
        }
        break;
    }
    }
}

void DrawRug(const Rug& rug)
{
    const Rectangle r{static_cast<float>(rug.x * TS + 4), static_cast<float>(rug.y * TS + 4), static_cast<float>(rug.w * TS - 8),
                      static_cast<float>(rug.h * TS - 8)};
    Box(r, rug.color, 0.06f);
    DrawRectangleRoundedLinesEx(Inset(r, 6), 0.06f, 6, 3.0f, Shade(rug.color, 1.35f));
    DrawRectangleRoundedLinesEx(Inset(r, 14), 0.06f, 6, 1.5f, Shade(rug.color, 0.75f));
    // Fringe on the short ends
    const bool wide = r.width >= r.height;
    for (float t = 4; t < (wide ? r.height : r.width) - 4; t += 5)
    {
        if (wide)
        {
            DrawLine(static_cast<int>(r.x - 4), static_cast<int>(r.y + t), static_cast<int>(r.x), static_cast<int>(r.y + t), Shade(rug.color, 1.5f));
            DrawLine(static_cast<int>(r.x + r.width), static_cast<int>(r.y + t), static_cast<int>(r.x + r.width + 4), static_cast<int>(r.y + t), Shade(rug.color, 1.5f));
        }
        else
        {
            DrawLine(static_cast<int>(r.x + t), static_cast<int>(r.y - 4), static_cast<int>(r.x + t), static_cast<int>(r.y), Shade(rug.color, 1.5f));
            DrawLine(static_cast<int>(r.x + t), static_cast<int>(r.y + r.height), static_cast<int>(r.x + t), static_cast<int>(r.y + r.height + 4), Shade(rug.color, 1.5f));
        }
    }
}

// ------------------------------------------------------------------ furniture

void DrawShelf(const Piece& p, RoomTheme theme, const ThemeStyle& st)
{
    const Rectangle r = Inset(p.r, 2);
    BoxOutlined(r, Shade(st.wood, 0.85f), Shade(st.wood, 0.55f), 0.1f);
    const Rectangle in = Inset(r, 4);
    DrawRectangleRec(in, Shade(st.wood, 0.45f));
    const bool horiz = Horizontal(p);
    const float len = horiz ? in.width : in.height;
    const float depth = horiz ? in.height : in.width;

    const bool books = theme == RoomTheme::Library || theme == RoomTheme::Study || theme == RoomTheme::Living ||
                       theme == RoomTheme::Music || theme == RoomTheme::Parlor || theme == RoomTheme::Hallway ||
                       theme == RoomTheme::Gallery || theme == RoomTheme::Foyer || theme == RoomTheme::GameRoom;
    const bool bottles = theme == RoomTheme::Cellar;
    const bool jars = theme == RoomTheme::Pantry || theme == RoomTheme::Kitchen;
    const bool toys = theme == RoomTheme::Nursery;

    if (books)
    {
        static const Color spines[] = {{150, 40, 40, 255}, {40, 90, 60, 255}, {40, 60, 120, 255}, {150, 110, 50, 255},
                                       {90, 50, 90, 255},  {190, 170, 130, 255}, {70, 40, 30, 255}};
        float t = 0;
        int i = 0;
        while (t < len - 3)
        {
            const float w = 4.0f + static_cast<float>(Hash(p.tx, p.ty, i) % 4);
            const float h = depth * (0.7f + 0.3f * Rand01(p.tx, p.ty, i + 50));
            const Color c = spines[Hash(p.tx + i, p.ty, 3) % 7];
            if (horiz)
                DrawRectangleRec({in.x + t, in.y + depth - h, w - 1, h}, c);
            else
                DrawRectangleRec({in.x + depth - h, in.y + t, h, w - 1}, c);
            t += w;
            ++i;
        }
    }
    else
    {
        // Rows of round items: bottles, jars, toys or cans.
        static const Color jarColors[] = {{200, 60, 40, 255}, {230, 180, 60, 255}, {220, 120, 40, 255}, {110, 150, 60, 255}, {160, 90, 60, 255}};
        static const Color bottleColors[] = {{40, 70, 45, 255}, {90, 25, 35, 255}, {60, 40, 30, 255}};
        static const Color toyColors[] = {{230, 90, 90, 255}, {90, 170, 230, 255}, {240, 200, 80, 255}, {130, 200, 120, 255}};
        static const Color canColors[] = {{150, 150, 160, 255}, {170, 90, 60, 255}, {90, 110, 150, 255}};
        const int rows = std::max(1, static_cast<int>(depth / 11));
        int i = 0;
        for (int row = 0; row < rows; ++row)
        {
            for (float t = 6; t < len - 4; t += 10, ++i)
            {
                const float d = (row + 0.5f) * depth / rows;
                const Vector2 c = horiz ? Vector2{in.x + t, in.y + d} : Vector2{in.x + d, in.y + t};
                Color col;
                if (bottles) col = bottleColors[Hash(i, p.tx, 5) % 3];
                else if (jars) col = jarColors[Hash(i, p.ty, 5) % 5];
                else if (toys) col = toyColors[Hash(i, p.tx, 5) % 4];
                else col = canColors[Hash(i, p.tx, 5) % 3];
                DrawCircleV(c, bottles ? 3.5f : 4.0f, col);
                DrawCircleV({c.x - 1, c.y - 1}, 1.2f, Fade(WHITE, 0.5f));
            }
        }
    }
}

void DrawChairs(const Piece& p, Rectangle table, const ThemeStyle& st)
{
    const Color chair = Shade(st.accent, 0.9f);
    if (Horizontal(p))
    {
        for (int i = 0; i < p.tw; ++i)
        {
            const float cx = p.r.x + i * TS + TS / 2.0f;
            Box({cx - 8, p.r.y + 1, 16, table.y - p.r.y - 2}, chair, 0.4f);
            Box({cx - 8, table.y + table.height + 1, 16, p.r.y + p.r.height - table.y - table.height - 2}, chair, 0.4f);
        }
    }
    else
    {
        for (int i = 0; i < p.th; ++i)
        {
            const float cy = p.r.y + i * TS + TS / 2.0f;
            Box({p.r.x + 1, cy - 8, table.x - p.r.x - 2, 16}, chair, 0.4f);
            Box({table.x + table.width + 1, cy - 8, p.r.x + p.r.width - table.x - table.width - 2, 16}, chair, 0.4f);
        }
    }
}

void DrawTable(const Piece& p, RoomTheme theme, const ThemeStyle& st)
{
    const Rectangle r = p.r;
    const Vector2 c{r.x + r.width / 2, r.y + r.height / 2};
    switch (theme)
    {
    case RoomTheme::Kitchen:
    {
        // Island / breakfast table with a stone top and a fruit bowl.
        const bool island = p.tw * p.th >= 12;
        const Rectangle top = Inset(r, island ? 2 : 8);
        if (!island) DrawChairs(p, top, st);
        BoxOutlined(top, island ? Color{196, 190, 178, 255} : st.wood, island ? Color{120, 116, 108, 255} : Shade(st.wood, 0.6f), 0.08f, 3);
        DrawCircleV(c, 12, Color{230, 230, 225, 255});
        DrawCircleV({c.x - 4, c.y - 2}, 4, RED);
        DrawCircleV({c.x + 4, c.y + 1}, 4, ORANGE);
        DrawCircleV({c.x, c.y + 5}, 4, YELLOW);
        return;
    }
    case RoomTheme::Dining:
    case RoomTheme::Ballroom:
    case RoomTheme::Conservatory:
    {
        // Dining table with chairs, plates and candles.
        const Rectangle top = Inset(r, 10);
        DrawChairs(p, top, st);
        BoxOutlined(top, st.wood, Shade(st.wood, 0.6f), 0.1f, 2);
        if (theme == RoomTheme::Dining && Horizontal(p))
            DrawRectangleRec({top.x + 10, c.y - 6, top.width - 20, 12}, st.accent); // table runner
        if (Horizontal(p))
            for (int i = 0; i < p.tw; ++i)
            {
                const float x = r.x + i * TS + TS / 2.0f;
                DrawCircleV({x, top.y + 7}, 5, Color{240, 238, 230, 255});
                DrawCircleV({x, top.y + top.height - 7}, 5, Color{240, 238, 230, 255});
            }
        DrawCircleV(c, 3, GOLD);
        DrawCircleV({c.x, c.y - 1}, 1.5f, Color{255, 220, 120, 255});
        return;
    }
    case RoomTheme::Study:
    case RoomTheme::Library:
    {
        // Desk: papers, a lamp and a book.
        BoxOutlined(Inset(r, 3), st.wood, Shade(st.wood, 0.6f), 0.08f, 2);
        DrawRectangleRec({c.x - 10, c.y - 6, 14, 10}, Color{235, 232, 220, 255});
        DrawRectangleRec({c.x - 7, c.y - 3, 14, 10}, Color{245, 242, 232, 255});
        DrawCircleV({r.x + 12, r.y + 12}, 6, Color{240, 210, 110, 255});
        DrawRectangleRec({r.x + r.width - 20, r.y + r.height - 16, 10, 8}, Color{120, 40, 40, 255});
        return;
    }
    case RoomTheme::GameRoom:
    {
        // Ping-pong table.
        const Rectangle top = Inset(r, 3);
        BoxOutlined(top, Color{40, 90, 150, 255}, WHITE, 0.03f, 2);
        DrawLineEx({c.x, top.y}, {c.x, top.y + top.height}, 3, Color{230, 230, 230, 255});
        DrawLineEx({top.x, c.y}, {top.x + top.width, c.y}, 1, Fade(WHITE, 0.7f));
        return;
    }
    default:
    {
        const bool small = p.tw * p.th == 1;
        const Rectangle top = Inset(r, small ? 6 : 4);
        if (theme == RoomTheme::Foyer || theme == RoomTheme::Parlor || theme == RoomTheme::Hallway)
        {
            DrawEllipse(static_cast<int>(c.x + 4), static_cast<int>(c.y + 5), top.width / 2, top.height / 2, Fade(BLACK, 0.25f));
            DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y), top.width / 2, top.height / 2, st.wood);
            DrawEllipseLines(static_cast<int>(c.x), static_cast<int>(c.y), top.width / 2, top.height / 2, Shade(st.wood, 0.6f));
        }
        else
        {
            BoxOutlined(top, st.wood, Shade(st.wood, 0.6f), 0.15f, 2);
        }
        if (small)
        {
            DrawCircleV(c, 7, Color{240, 220, 150, 255}); // lamp
            DrawCircleV(c, 3, Color{255, 245, 200, 255});
        }
        else
        {
            // Vase of flowers.
            DrawCircleV(c, 6, Color{70, 110, 150, 255});
            DrawCircleV({c.x - 4, c.y - 4}, 3, Color{230, 80, 110, 255});
            DrawCircleV({c.x + 4, c.y - 3}, 3, Color{250, 220, 90, 255});
            DrawCircleV({c.x, c.y + 4}, 3, Color{240, 240, 240, 255});
        }
        return;
    }
    }
}

void DrawCounter(const Level& level, const Piece& p, RoomTheme theme, const ThemeStyle& st)
{
    const Rectangle r = Inset(p.r, 1);
    switch (theme)
    {
    case RoomTheme::Theater:
    {
        // The big screen.
        DrawRectangleRec(r, Color{20, 20, 26, 255});
        DrawRectangleGradientV(static_cast<int>(r.x + 6), static_cast<int>(r.y + 6), static_cast<int>(r.width - 12), static_cast<int>(r.height - 12),
                               Color{150, 190, 230, 255}, Color{90, 120, 170, 255});
        return;
    }
    case RoomTheme::Gallery:
    {
        // Glass display case with a trophy inside.
        BoxOutlined(r, Color{40, 40, 46, 255}, Color{120, 110, 90, 255}, 0.1f, 2);
        DrawRectangleRec(Inset(r, 5), Fade(Color{170, 210, 230, 255}, 0.45f));
        const Vector2 c{r.x + r.width / 2, r.y + r.height / 2};
        DrawCircleV(c, 6, GOLD);
        DrawCircleV({c.x - 2, c.y - 2}, 2, Color{255, 240, 180, 255});
        return;
    }
    case RoomTheme::GameRoom:
    {
        // TV stand with a TV.
        BoxOutlined(r, Color{40, 36, 40, 255}, Color{80, 76, 84, 255}, 0.1f, 2);
        DrawRectangleRec({r.x + 8, r.y + r.height / 2 - 4, r.width - 16, 8}, Color{15, 15, 20, 255});
        DrawRectangleRec({r.x + 10, r.y + r.height / 2 - 2, r.width - 20, 4}, Color{60, 120, 200, 255});
        return;
    }
    case RoomTheme::Billiards:
    {
        // Bar with stools.
        BoxOutlined(r, Shade(st.wood, 0.8f), Shade(st.wood, 0.5f), 0.1f, 2);
        DrawRectangleRec(Inset(r, 5), Shade(st.wood, 1.2f));
        for (int i = 0; i < std::max(p.tw, p.th); ++i)
        {
            const Vector2 c = Horizontal(p) ? Vector2{r.x + i * TS + TS / 2.0f, r.y + r.height / 2} : Vector2{r.x + r.width / 2, r.y + i * TS + TS / 2.0f};
            DrawCircleV(c, 5, Color{230, 200, 150, 255});
        }
        return;
    }
    case RoomTheme::Bathroom:
    {
        // Vanity with sinks.
        BoxOutlined(r, Color{220, 216, 206, 255}, Color{150, 146, 138, 255}, 0.1f, 2);
        for (int i = 0; i < std::max(p.tw, p.th); i += 2)
        {
            const Vector2 c = Horizontal(p) ? Vector2{r.x + i * TS + TS, r.y + r.height / 2} : Vector2{r.x + r.width / 2, r.y + i * TS + TS};
            DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y), 12, 8, Color{245, 248, 250, 255});
            DrawEllipseLines(static_cast<int>(c.x), static_cast<int>(c.y), 12, 8, Color{160, 170, 180, 255});
            DrawCircleV({c.x, c.y - 6}, 2, Color{170, 175, 185, 255});
        }
        return;
    }
    default:
    {
        // Counter / cabinet: stone or wood top with drawer lines along the front.
        const bool stone = theme == RoomTheme::Kitchen || theme == RoomTheme::Laundry;
        const Color top = stone ? Color{186, 182, 172, 255} : Shade(st.wood, 1.1f);
        BoxOutlined(r, top, stone ? Color{120, 116, 108, 255} : Shade(st.wood, 0.6f), 0.08f, 2);
        const bool horiz = Horizontal(p);
        for (int i = 1; i < std::max(p.tw, p.th); ++i)
        {
            if (horiz)
                DrawLine(static_cast<int>(r.x + i * TS), static_cast<int>(r.y + 3), static_cast<int>(r.x + i * TS), static_cast<int>(r.y + r.height - 3), Fade(BLACK, 0.2f));
            else
                DrawLine(static_cast<int>(r.x + 3), static_cast<int>(r.y + i * TS), static_cast<int>(r.x + r.width - 3), static_cast<int>(r.y + i * TS), Fade(BLACK, 0.2f));
        }
        if (theme == RoomTheme::Kitchen && p.index == 1 && std::max(p.tw, p.th) >= 3)
        {
            // Sink in the middle of the second counter.
            const Vector2 c = horiz ? Vector2{r.x + r.width / 2, r.y + r.height / 2} : Vector2{r.x + r.width / 2, r.y + r.height / 2};
            DrawRectangleRounded({c.x - 14, c.y - 10, 28, 20}, 0.3f, 4, Color{150, 156, 164, 255});
            DrawRectangleRounded({c.x - 11, c.y - 7, 22, 14}, 0.3f, 4, Color{110, 116, 124, 255});
        }
        (void)level;
        return;
    }
    }
}

void DrawAppliance(const Piece& p, RoomTheme theme)
{
    const Color white{226, 228, 230, 255};
    const Color steel{190, 196, 202, 255};
    for (int j = 0; j < p.th; ++j)
    {
        for (int i = 0; i < p.tw; ++i)
        {
            if ((theme == RoomTheme::Kitchen || theme == RoomTheme::Basement) && p.tw * p.th > 1) break; // drawn as one piece below
            const Rectangle t = Inset({p.r.x + i * TS, p.r.y + j * TS, static_cast<float>(TS), static_cast<float>(TS)}, 2);
            const Vector2 c{t.x + t.width / 2, t.y + t.height / 2};
            switch (theme)
            {
            case RoomTheme::Kitchen: // stove
                BoxOutlined(t, Color{50, 50, 54, 255}, Color{90, 90, 96, 255}, 0.1f, 2);
                for (int k = 0; k < 4; ++k)
                {
                    const Vector2 b{c.x + ((k % 2) ? 8.0f : -8.0f), c.y + ((k / 2) ? 8.0f : -8.0f)};
                    DrawRing(b, 3, 6, 0, 360, 16, Color{140, 140, 146, 255});
                }
                break;
            case RoomTheme::Laundry: // washer / dryer
                BoxOutlined(t, white, Color{150, 154, 160, 255}, 0.1f, 2);
                DrawCircleV(c, 12, Color{120, 130, 140, 255});
                DrawCircleV(c, 9, (i + j) % 2 ? Color{60, 110, 170, 255} : Color{170, 200, 230, 255});
                DrawCircleV({t.x + 6, t.y + 5}, 2, Color{90, 90, 90, 255});
                break;
            case RoomTheme::Bathroom: // toilet
                DrawRectangleRounded({t.x + 6, t.y + 1, t.width - 12, 10}, 0.4f, 4, white);
                DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y + 5), 11, 13, white);
                DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y + 6), 7, 9, Color{170, 200, 215, 255});
                break;
            case RoomTheme::GameRoom: // arcade cabinet
            {
                static const Color cabinets[] = {{170, 40, 60, 255}, {40, 90, 170, 255}, {60, 140, 70, 255}, {150, 80, 170, 255}};
                BoxOutlined(t, cabinets[(i + p.index) % 4], Color{20, 20, 20, 255}, 0.1f, 2);
                DrawRectangleRec({t.x + 6, t.y + 5, t.width - 12, 14}, Color{20, 30, 40, 255});
                DrawRectangleRec({t.x + 8, t.y + 7, t.width - 16, 10}, Color{100, 220, 200, 255});
                DrawCircleV({c.x - 5, t.y + t.height - 8}, 3, RED);
                DrawCircleV({c.x + 5, t.y + t.height - 8}, 3, YELLOW);
                break;
            }
            case RoomTheme::Basement: // furnace / water heater
                if (p.tw * p.th > 1)
                {
                    BoxOutlined(t, Color{70, 70, 76, 255}, Color{40, 40, 44, 255}, 0.05f, 2);
                    DrawRectangleRec({t.x + 8, t.y + 12, t.width - 16, 10}, Color{230, 120, 40, 255});
                }
                else
                {
                    DrawCircleV(c, 16, Color{170, 175, 180, 255});
                    DrawCircleV(c, 12, Color{140, 146, 152, 255});
                }
                break;
            case RoomTheme::Garage: // lawnmower
                BoxOutlined(Inset(t, 4), Color{190, 40, 40, 255}, Color{90, 20, 20, 255}, 0.3f, 2);
                DrawCircleV(c, 6, Color{40, 40, 40, 255});
                break;
            default:
                BoxOutlined(t, white, Color{150, 154, 160, 255}, 0.1f, 2);
                break;
            }
        }
    }
    if (theme == RoomTheme::Basement && p.tw * p.th > 1)
    {
        // Furnace: one big unit with vents and a glowing burner window.
        const Rectangle r = Inset(p.r, 3);
        BoxOutlined(r, Color{72, 72, 78, 255}, Color{40, 40, 44, 255}, 0.06f, 2);
        for (float y = r.y + 8; y < r.y + r.height * 0.5f; y += 6)
            DrawLine(static_cast<int>(r.x + 10), static_cast<int>(y), static_cast<int>(r.x + r.width - 10), static_cast<int>(y), Color{50, 50, 54, 255});
        const Rectangle glow{r.x + r.width / 2 - 16, r.y + r.height * 0.62f, 32, 16};
        DrawRectangleRec(glow, Color{40, 20, 10, 255});
        DrawRectangleRec(Inset(glow, 3), Color{240, 130, 40, 255});
        DrawCircleV({r.x + r.width - 14, r.y + r.height - 14}, 5, Color{150, 150, 156, 255}); // dial
        DrawCircleV({r.x + 14, r.y + 14}, 9, Color{120, 120, 126, 255}); // flue
        DrawCircleV({r.x + 14, r.y + 14}, 5, Color{40, 40, 44, 255});
    }
    if (theme == RoomTheme::Kitchen && p.tw * p.th > 1)
    {
        // Fridge: steel doors and handles.
        const Rectangle r = Inset(p.r, 2);
        BoxOutlined(r, steel, Color{120, 126, 132, 255}, 0.08f, 2);
        if (Horizontal(p))
        {
            DrawLine(static_cast<int>(r.x + r.width / 2), static_cast<int>(r.y + 2), static_cast<int>(r.x + r.width / 2), static_cast<int>(r.y + r.height - 2), Color{120, 126, 132, 255});
            DrawRectangleRec({r.x + r.width / 2 - 6, r.y + r.height - 12, 4, 8}, Color{90, 96, 102, 255});
            DrawRectangleRec({r.x + r.width / 2 + 2, r.y + r.height - 12, 4, 8}, Color{90, 96, 102, 255});
        }
        else
        {
            DrawLine(static_cast<int>(r.x + 2), static_cast<int>(r.y + r.height / 3), static_cast<int>(r.x + r.width - 2), static_cast<int>(r.y + r.height / 3), Color{120, 126, 132, 255});
            DrawRectangleRec({r.x + 4, r.y + r.height / 3 + 6, 4, 12}, Color{90, 96, 102, 255});
        }
    }
}

void DrawSeat(const Level& level, const Piece& p, RoomTheme theme, const ThemeStyle& st)
{
    const Rectangle r = Inset(p.r, 3);
    const Side back = SeatBackSide(level, p, theme);
    const bool alongX = back == Side::Top || back == Side::Bottom;
    const int seats = alongX ? p.tw : p.th;

    if (theme == RoomTheme::Conservatory)
    {
        // Garden bench: wooden slats.
        Box(r, st.wood, 0.1f);
        for (int i = 1; i < 4; ++i)
        {
            if (alongX)
                DrawLine(static_cast<int>(r.x + 2), static_cast<int>(r.y + i * r.height / 4), static_cast<int>(r.x + r.width - 2), static_cast<int>(r.y + i * r.height / 4), Shade(st.wood, 0.6f));
            else
                DrawLine(static_cast<int>(r.x + i * r.width / 4), static_cast<int>(r.y + 2), static_cast<int>(r.x + i * r.width / 4), static_cast<int>(r.y + r.height - 2), Shade(st.wood, 0.6f));
        }
        return;
    }
    if (theme == RoomTheme::GameRoom && p.tw * p.th <= 2)
    {
        // Bean bags.
        for (int i = 0; i < p.tw * p.th; ++i)
        {
            const Vector2 c = alongX ? Vector2{r.x + i * TS + TS / 2.0f - 3, r.y + r.height / 2} : Vector2{r.x + r.width / 2, r.y + i * TS + TS / 2.0f - 3};
            DrawCircleV(c, 15, i % 2 ? Color{220, 140, 40, 255} : Color{60, 150, 200, 255});
            DrawCircleV({c.x - 4, c.y - 4}, 5, Fade(WHITE, 0.25f));
        }
        return;
    }

    const Color body = theme == RoomTheme::Nursery && p.tw * p.th <= 2 ? st.wood : st.accent;
    Box(r, Shade(body, 0.8f), 0.3f);
    const float backT = 9.0f;
    Box(Strip(r, back, backT), Shade(body, 0.65f), 0.5f);
    // Seat cushions, one per tile, plus armrests at each end.
    for (int i = 0; i < seats; ++i)
    {
        Rectangle cushion;
        if (alongX)
        {
            const float y0 = back == Side::Top ? r.y + backT : r.y;
            cushion = {r.x + i * TS + (i == 0 ? 6.0f : 1.0f), y0 + 1, TS - (i == 0 ? 7.0f : 2.0f) - (i == seats - 1 ? 6.0f : 0.0f), r.height - backT - 2};
        }
        else
        {
            const float x0 = back == Side::Left ? r.x + backT : r.x;
            cushion = {x0 + 1, r.y + i * TS + (i == 0 ? 6.0f : 1.0f), r.width - backT - 2, TS - (i == 0 ? 7.0f : 2.0f) - (i == seats - 1 ? 6.0f : 0.0f)};
        }
        Box(cushion, body, 0.3f);
        if (theme == RoomTheme::Theater && i < seats - 1)
        {
            const Vector2 cup = alongX ? Vector2{r.x + (i + 1) * TS, r.y + r.height / 2 + 4} : Vector2{r.x + r.width / 2, r.y + (i + 1) * TS};
            DrawCircleV(cup, 3, Color{30, 30, 30, 255});
        }
    }
}

void DrawBed(const Level& level, const Piece& p, RoomTheme theme, const ThemeStyle& st)
{
    const Rectangle r = Inset(p.r, 2);
    const Side head = HeadSide(level, p);
    const bool crib = theme == RoomTheme::Nursery;
    BoxOutlined(r, st.wood, Shade(st.wood, 0.6f), 0.08f, 2);
    const Rectangle mattress = Inset(r, 5);
    DrawRectangleRec(mattress, Color{235, 232, 225, 255});
    // Blanket covers the foot two-thirds.
    Rectangle blanket = mattress;
    const float pillowZone = (head == Side::Top || head == Side::Bottom) ? mattress.height * 0.3f : mattress.width * 0.3f;
    switch (head)
    {
    case Side::Top: blanket.y += pillowZone; blanket.height -= pillowZone; break;
    case Side::Bottom: blanket.height -= pillowZone; break;
    case Side::Left: blanket.x += pillowZone; blanket.width -= pillowZone; break;
    case Side::Right: blanket.width -= pillowZone; break;
    }
    DrawRectangleRec(blanket, crib ? Color{150, 200, 230, 255} : st.accent);
    DrawRectangleRec(Strip(blanket, head, 6), Shade(crib ? Color{150, 200, 230, 255} : st.accent, 1.3f)); // folded edge
    // Pillows
    const int pillows = (head == Side::Top || head == Side::Bottom) ? std::max(1, p.tw / 2) : std::max(1, p.th / 2);
    for (int i = 0; i < pillows; ++i)
    {
        Rectangle pr;
        if (head == Side::Top || head == Side::Bottom)
        {
            const float w = mattress.width / pillows;
            pr = {mattress.x + i * w + 4, head == Side::Top ? mattress.y + 3 : mattress.y + mattress.height - pillowZone + 3, w - 8, pillowZone - 6};
        }
        else
        {
            const float h = mattress.height / pillows;
            pr = {head == Side::Left ? mattress.x + 3 : mattress.x + mattress.width - pillowZone + 3, mattress.y + i * h + 4, pillowZone - 6, h - 8};
        }
        Box(pr, WHITE, 0.4f);
    }
    if (crib)
        for (float x = r.x + 6; x < r.x + r.width - 4; x += 8)
            DrawLine(static_cast<int>(x), static_cast<int>(r.y + 1), static_cast<int>(x), static_cast<int>(r.y + 5), Shade(st.wood, 0.6f));
}

void DrawPoolTable(const Piece& p, RoomTheme theme, const ThemeStyle& st)
{
    const Rectangle r = Inset(p.r, 3);
    BoxOutlined(r, Shade(st.wood, 0.9f), Shade(st.wood, 0.5f), 0.12f, 2);
    const Rectangle felt = Inset(r, 8);
    DrawRectangleRec(felt, theme == RoomTheme::GameRoom ? Color{40, 90, 150, 255} : Color{40, 120, 70, 255});
    const Vector2 pockets[] = {{felt.x, felt.y}, {felt.x + felt.width, felt.y}, {felt.x, felt.y + felt.height},
                               {felt.x + felt.width, felt.y + felt.height}, {felt.x + felt.width / 2, felt.y}, {felt.x + felt.width / 2, felt.y + felt.height}};
    for (const Vector2& pk : pockets) DrawCircleV(pk, 5, Color{15, 15, 15, 255});
    static const Color balls[] = {WHITE, YELLOW, BLUE, RED, PURPLE, ORANGE, DARKGREEN, MAROON, BLACK};
    for (int i = 0; i < 6; ++i)
    {
        const Vector2 b{felt.x + 12 + Rand01(p.tx, i, 31) * (felt.width - 24), felt.y + 10 + Rand01(p.ty, i, 32) * (felt.height - 20)};
        DrawCircleV(b, 4, balls[(i + p.index) % 9]);
    }
}

void DrawTub(const Piece& p, RoomTheme theme)
{
    const Rectangle r = Inset(p.r, 3);
    const Vector2 c{r.x + r.width / 2, r.y + r.height / 2};
    if (theme == RoomTheme::Conservatory)
    {
        // Fountain
        DrawCircleV(c, std::min(r.width, r.height) / 2, Color{150, 146, 136, 255});
        DrawCircleV(c, std::min(r.width, r.height) / 2 - 8, Color{70, 130, 170, 255});
        DrawCircleV(c, 14, Color{150, 146, 136, 255});
        DrawCircleV(c, 8, Color{120, 180, 210, 255});
        for (int k = 0; k < 8; ++k)
        {
            const float a = k * PI / 4;
            DrawCircleV({c.x + std::cos(a) * 30, c.y + std::sin(a) * 30}, 2, Fade(WHITE, 0.6f));
        }
        return;
    }
    BoxOutlined(r, Color{235, 238, 240, 255}, Color{160, 170, 178, 255}, 0.4f, 2);
    DrawRectangleRounded(Inset(r, 7), 0.5f, 6, Color{150, 200, 225, 255});
    DrawCircleV({r.x + 10, c.y}, 3, Color{170, 176, 184, 255});
}

void DrawPlant(const Piece& p, RoomTheme theme)
{
    const Color leafA{60, 130, 70, 255};
    const Color leafB{90, 160, 80, 255};
    if (p.tw * p.th > 1)
    {
        // Planter bed with shrubs and flowers.
        const Rectangle r = Inset(p.r, 2);
        BoxOutlined(r, Color{110, 76, 50, 255}, Color{70, 46, 30, 255}, 0.08f, 3);
        DrawRectangleRec(Inset(r, 5), Color{80, 60, 40, 255});
        for (int j = 0; j < p.th * 2; ++j)
            for (int i = 0; i < p.tw * 2; ++i)
            {
                const Vector2 c{r.x + 10 + i * 20 - 2 + Rand01(i, j, 41) * 4, r.y + 10 + j * 20 - 2 + Rand01(i, j, 42) * 4};
                DrawCircleV(c, 9, (i + j) % 2 ? leafA : leafB);
                if (Hash(i, j, 43 + p.index) % 3 == 0) DrawCircleV({c.x + 2, c.y - 2}, 3, (Hash(i, j, 44) % 2) ? Color{230, 90, 120, 255} : Color{250, 220, 90, 255});
            }
        return;
    }
    const Vector2 c{p.r.x + TS / 2.0f, p.r.y + TS / 2.0f};
    DrawCircleV(c, 12, Color{170, 90, 60, 255});
    DrawCircleV(c, 9, Color{90, 60, 40, 255});
    for (int k = 0; k < 6; ++k)
    {
        const float a = k * PI / 3 + p.index;
        DrawCircleV({c.x + std::cos(a) * 9, c.y + std::sin(a) * 9}, 7, k % 2 ? leafA : leafB);
    }
    DrawCircleV(c, 6, leafB);
    (void)theme;
}

void DrawBoxes(const Piece& p, RoomTheme theme)
{
    for (int j = 0; j < p.th; ++j)
    {
        for (int i = 0; i < p.tw; ++i)
        {
            const int tx = p.tx + i;
            const int ty = p.ty + j;
            const Rectangle t{p.r.x + i * TS, p.r.y + j * TS, static_cast<float>(TS), static_cast<float>(TS)};
            const Vector2 c{t.x + TS / 2.0f, t.y + TS / 2.0f};
            if (theme == RoomTheme::Cellar)
            {
                // Barrel
                DrawCircleV(c, 17, Color{120, 76, 40, 255});
                DrawRing(c, 12, 14, 0, 360, 24, Color{70, 64, 60, 255});
                DrawCircleV(c, 6, Color{100, 62, 32, 255});
            }
            else if (theme == RoomTheme::Nursery)
            {
                // Toy block
                static const Color toy[] = {{230, 90, 90, 255}, {90, 170, 230, 255}, {240, 200, 80, 255}, {130, 200, 120, 255}};
                const Color col = toy[Hash(tx, ty, 51) % 4];
                const Rectangle b = Inset(t, 5);
                Box(b, col, 0.1f);
                DrawRectangleRec(Inset(b, 7), Shade(col, 1.25f));
            }
            else
            {
                // Cardboard box, slightly jittered
                const float jx = Rand01(tx, ty, 52) * 4 - 2;
                const float jy = Rand01(tx, ty, 53) * 4 - 2;
                const Rectangle b{t.x + 3 + jx, t.y + 3 + jy, TS - 6.0f, TS - 6.0f};
                const Color col = Mix(Color{170, 130, 80, 255}, Color{140, 104, 64, 255}, Rand01(tx, ty, 54));
                BoxOutlined(b, col, Shade(col, 0.7f), 0.05f, 1.5f);
                DrawRectangleRec({b.x + b.width / 2 - 3, b.y, 6, b.height}, Fade(Color{220, 200, 150, 255}, 0.6f)); // tape
            }
        }
    }
}

void FillPolygon(Vector2 center, const std::vector<Vector2>& pts, Color color)
{
    for (size_t i = 0; i < pts.size(); ++i)
    {
        Vector2 a = pts[i];
        Vector2 b = pts[(i + 1) % pts.size()];
        // raylib wants counter-clockwise triangles; flip if needed.
        const float cross = (a.x - center.x) * (b.y - center.y) - (a.y - center.y) * (b.x - center.x);
        if (cross > 0.0f) std::swap(a, b);
        DrawTriangle(center, a, b, color);
    }
}

void DrawPiano(const Piece& p)
{
    // Top-down grand piano: a straight keyboard edge along the bottom and a curved tail at the top.
    const Rectangle r = Inset(p.r, 3);
    auto shape = [&](float inset) {
        const float x = r.x + inset, y = r.y + inset, W = r.width - 2 * inset, H = r.height - 2 * inset;
        std::vector<Vector2> pts;
        pts.push_back({x, y + H});
        pts.push_back({x + W, y + H});
        // Long curved side: quarter ellipse from the right edge up to the tail.
        for (int k = 0; k <= 16; ++k)
        {
            const float a = k / 16.0f * (PI / 2);
            pts.push_back({x + W * 0.4f + W * 0.6f * std::cos(a), y + H * 0.55f - H * 0.55f * std::sin(a)});
        }
        // Short curve back down the left side.
        for (int k = 1; k <= 10; ++k)
        {
            const float a = PI / 2 + k / 10.0f * (PI / 2);
            pts.push_back({x + W * 0.4f + W * 0.4f * std::cos(a), y + H * 0.35f - H * 0.35f * std::sin(a)});
        }
        return pts;
    };
    const Vector2 center{r.x + r.width * 0.5f, r.y + r.height * 0.6f};
    FillPolygon({center.x + 4, center.y + 5}, [&] { auto s = shape(0); for (auto& v : s) { v.x += 4; v.y += 5; } return s; }(), Fade(BLACK, 0.25f));
    FillPolygon(center, shape(0), Color{22, 20, 22, 255});
    // Lid propped open: warm soundboard with gold strings.
    FillPolygon(center, shape(9), Color{150, 108, 60, 255});
    for (float x = r.x + 16; x < r.x + r.width * 0.7f; x += 5)
        DrawLineEx({x, r.y + r.height - 20}, {x + r.width * 0.12f, r.y + r.height * 0.3f}, 1.0f, Fade(GOLD, 0.55f));
    // Keyboard
    const Rectangle keys{r.x + 6, r.y + r.height - 14, r.width - 12, 11};
    DrawRectangleRec(keys, Color{240, 238, 230, 255});
    for (float t = 3; t < keys.width - 3; t += 6)
    {
        const int n = static_cast<int>(t / 6) % 7;
        if (n == 2 || n == 6) continue;
        DrawRectangleRec({keys.x + t, keys.y, 3, 6}, BLACK);
    }
}

void DrawFireplace(const Level& level, const Piece& p)
{
    const Rectangle r = Inset(p.r, 1);
    const Color brick{150, 64, 48, 255};
    DrawRectangleRec(r, brick);
    for (float y = r.y + 6; y < r.y + r.height; y += 8)
        DrawLine(static_cast<int>(r.x), static_cast<int>(y), static_cast<int>(r.x + r.width), static_cast<int>(y), Shade(brick, 0.7f));
    // Firebox opens away from the wall.
    const Side wall = HeadSide(level, p);
    Rectangle box = Inset(r, 8);
    switch (wall)
    {
    case Side::Top: box.y += 4; box.height += 4; break;
    case Side::Bottom: box.y -= 4; box.height += 4; break;
    case Side::Left: box.x += 4; box.width += 4; break;
    case Side::Right: box.x -= 4; box.width += 4; break;
    }
    DrawRectangleRec(box, Color{30, 20, 18, 255});
    const Vector2 c{box.x + box.width / 2, box.y + box.height / 2};
    DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y), box.width * 0.35f, box.height * 0.35f, Color{240, 120, 40, 255});
    DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y), box.width * 0.2f, box.height * 0.2f, Color{255, 210, 90, 255});
}

void DrawWardrobe(const Piece& p, const ThemeStyle& st)
{
    const Rectangle r = Inset(p.r, 2);
    BoxOutlined(r, st.wood, Shade(st.wood, 0.55f), 0.06f, 2);
    const bool horiz = Horizontal(p);
    const int doors = std::max(2, horiz ? p.tw : p.th);
    for (int i = 0; i < doors; ++i)
    {
        Rectangle d = horiz ? Rectangle{r.x + 4 + i * (r.width - 8) / doors, r.y + 4, (r.width - 8) / doors - 3, r.height - 8}
                            : Rectangle{r.x + 4, r.y + 4 + i * (r.height - 8) / doors, r.width - 8, (r.height - 8) / doors - 3};
        DrawRectangleRec(d, Shade(st.wood, 1.15f));
        const Vector2 knob = horiz ? Vector2{d.x + (i % 2 ? 4.0f : d.width - 4), d.y + d.height / 2} : Vector2{d.x + d.width / 2, d.y + (i % 2 ? 4.0f : d.height - 4)};
        DrawCircleV(knob, 2, GOLD);
    }
}

void DrawCar(const Piece& p)
{
    static const Color bodies[] = {{170, 40, 40, 255}, {40, 80, 150, 255}, {60, 60, 66, 255}};
    const Color body = bodies[p.index % 3];
    const Rectangle r = Inset(p.r, 4);
    const bool vertical = !Horizontal(p);
    // Wheels
    const float wl = 10, ww = 18;
    if (vertical)
    {
        for (float y : {r.y + 14, r.y + r.height - 14 - ww})
        {
            DrawRectangleRec({r.x - 3, y, wl, ww}, BLACK);
            DrawRectangleRec({r.x + r.width - wl + 3, y, wl, ww}, BLACK);
        }
    }
    DrawRectangleRounded(r, 0.35f, 8, body);
    DrawRectangleRounded(Inset(r, 8), 0.3f, 8, Shade(body, 1.15f)); // roof
    // Windshield and rear window
    const Color glass{60, 80, 100, 255};
    if (vertical)
    {
        DrawRectangleRounded({r.x + 8, r.y + r.height * 0.22f, r.width - 16, 16}, 0.3f, 4, glass);
        DrawRectangleRounded({r.x + 10, r.y + r.height * 0.72f, r.width - 20, 12}, 0.3f, 4, glass);
        DrawCircleV({r.x + 10, r.y + 5}, 4, Color{255, 240, 170, 255});
        DrawCircleV({r.x + r.width - 10, r.y + 5}, 4, Color{255, 240, 170, 255});
    }
    else
    {
        DrawRectangleRounded({r.x + r.width * 0.22f, r.y + 8, 16, r.height - 16}, 0.3f, 4, glass);
        DrawRectangleRounded({r.x + r.width * 0.72f, r.y + 10, 12, r.height - 20}, 0.3f, 4, glass);
    }
}

// ------------------------------------------------------------------ Egypt wing (World 2)

const Color kSand{198, 170, 120, 255};
const Color kSandDark{150, 124, 84, 255};
const Color kGold{232, 192, 72, 255};
const Color kGoldDark{170, 128, 40, 255};
const Color kLapis{40, 72, 152, 255};
const Color kTurquoise{54, 168, 164, 255};
const Color kOnyx{38, 36, 46, 255};

void Ellipse(Vector2 c, float along, float across, bool horiz, Color col)
{
    if (horiz) DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y), along, across, col);
    else DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y), across, along, col);
}

// Tiny carved hieroglyphs: eye, ankh, water, bird, sun, scarab.
void DrawGlyph(float cx, float cy, int kind, Color c)
{
    switch (kind % 6)
    {
    case 0: // eye of Horus
        DrawEllipseLines(static_cast<int>(cx), static_cast<int>(cy), 7, 4, c);
        DrawCircleV({cx, cy}, 2.2f, c);
        DrawLineEx({cx - 2, cy + 4}, {cx - 4, cy + 9}, 1.5f, c);
        break;
    case 1: // ankh
        DrawRing({cx, cy - 4}, 2.5f, 4.5f, 0, 360, 16, c);
        DrawLineEx({cx, cy}, {cx, cy + 10}, 2.0f, c);
        DrawLineEx({cx - 5, cy + 3}, {cx + 5, cy + 3}, 2.0f, c);
        break;
    case 2: // water ripples
        for (int k = 0; k < 3; ++k)
            DrawLineEx({cx - 7, cy - 4 + k * 5.0f}, {cx - 3.5f, cy - 6 + k * 5.0f}, 1.5f, c),
                DrawLineEx({cx - 3.5f, cy - 6 + k * 5.0f}, {cx, cy - 4 + k * 5.0f}, 1.5f, c),
                DrawLineEx({cx, cy - 4 + k * 5.0f}, {cx + 3.5f, cy - 6 + k * 5.0f}, 1.5f, c),
                DrawLineEx({cx + 3.5f, cy - 6 + k * 5.0f}, {cx + 7, cy - 4 + k * 5.0f}, 1.5f, c);
        break;
    case 3: // bird
        DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 1), 6, 3, c);
        DrawCircleV({cx + 6, cy - 3}, 2.5f, c);
        DrawLineEx({cx + 8, cy - 3}, {cx + 11, cy - 1}, 1.5f, c);
        DrawLineEx({cx - 5, cy + 3}, {cx - 9, cy + 7}, 1.5f, c);
        DrawLineEx({cx, cy + 4}, {cx, cy + 9}, 1.5f, c);
        break;
    case 4: // sun disc
        DrawRing({cx, cy}, 3.0f, 5.0f, 0, 360, 20, c);
        DrawCircleV({cx, cy}, 1.6f, c);
        break;
    default: // scarab
        DrawEllipse(static_cast<int>(cx), static_cast<int>(cy), 4, 6, c);
        DrawLineEx({cx, cy - 6}, {cx, cy + 6}, 1.0f, kOnyx);
        for (int k = -1; k <= 1; ++k)
        {
            DrawLineEx({cx - 4, cy + k * 4.0f}, {cx - 8, cy + k * 4.0f + 2}, 1.5f, c);
            DrawLineEx({cx + 4, cy + k * 4.0f}, {cx + 8, cy + k * 4.0f + 2}, 1.5f, c);
        }
        break;
    }
}

// One column seen from above: fluted drum with a lotus capital.
void DrawPillarTile(Vector2 c, Color stone)
{
    DrawEllipse(static_cast<int>(c.x + 3), static_cast<int>(c.y + 5), 18, 17, Fade(BLACK, 0.3f));
    DrawRectangleRounded({c.x - 17, c.y - 17, 34, 34}, 0.18f, 4, Shade(stone, 0.5f)); // square plinth
    DrawCircleV(c, 15.5f, Shade(stone, 0.42f));
    DrawCircleV(c, 14.5f, Shade(stone, 0.95f));
    for (int k = 0; k < 12; ++k)
    {
        const float a = k * 2 * PI / 12;
        DrawLineEx({c.x + std::cos(a) * 6, c.y + std::sin(a) * 6}, {c.x + std::cos(a) * 14, c.y + std::sin(a) * 14}, 1.5f, Shade(stone, 0.72f));
    }
    DrawRing(c, 8.5f, 11, 0, 360, 24, Shade(stone, 1.15f)); // lotus capital
    for (int k = 0; k < 8; ++k)
    {
        const float a = k * 2 * PI / 8 + 0.2f;
        DrawCircleV({c.x + std::cos(a) * 9.7f, c.y + std::sin(a) * 9.7f}, 2.0f, kGold);
    }
    DrawCircleV(c, 6, Shade(stone, 1.2f));
    DrawCircleV(c, 2.5f, kGoldDark);
}

void DrawStele(const Piece& p, const ThemeStyle& st)
{
    const Rectangle r = Inset(p.r, 2);
    BoxOutlined(r, kSand, kSandDark, 0.08f, 2);
    DrawRectangleRec(Strip(r, Horizontal(p) ? Side::Top : Side::Left, 4), kGold);
    const Rectangle panel = Inset(r, 6);
    DrawRectangleRec(panel, Shade(kSand, 0.86f));
    int n = 0;
    for (float y = panel.y + 10; y < panel.y + panel.height - 4; y += 16)
    {
        for (float x = panel.x + 10; x < panel.x + panel.width - 4; x += 16)
        {
            DrawGlyph(x, y, static_cast<int>(Hash(p.tx * 7 + n, p.ty * 3 + n, 5)), (n % 3 == 0) ? kLapis : Shade(kSandDark, 0.7f));
            ++n;
        }
    }
    (void)st;
}

void DrawAltar(const Piece& p)
{
    const Rectangle r = Inset(p.r, 3);
    BoxOutlined(r, kSand, kSandDark, 0.1f, 2);
    DrawRectangleLinesEx(Inset(r, 5), 2, kGold);
    const bool horiz = Horizontal(p);
    const Rectangle runner = horiz ? Rectangle{r.x + 8, r.y + r.height / 2 - 4, r.width - 16, 8} : Rectangle{r.x + r.width / 2 - 4, r.y + 8, 8, r.height - 16};
    DrawRectangleRec(runner, kLapis);
    for (int j = 0; j < p.th; ++j)
        for (int i = 0; i < p.tw; ++i)
        {
            const Vector2 c{p.r.x + i * TS + TS / 2.0f, p.r.y + j * TS + TS / 2.0f};
            DrawCircleV(c, 8, kGoldDark);
            DrawCircleV(c, 6, kGold);
            DrawCircleV(c, 3.5f, (i + j + p.index) % 2 ? Color{190, 60, 50, 255} : Color{70, 150, 80, 255});
        }
}

void DrawSarcophagus(const Piece& p)
{
    const bool horiz = Horizontal(p);
    const Rectangle r = Inset(p.r, 3);
    BoxOutlined(r, kSandDark, kOnyx, 0.12f, 2);
    const Rectangle lid = Inset(r, 5);
    Box(lid, kGold, 0.35f);
    const Vector2 c{lid.x + lid.width / 2, lid.y + lid.height / 2};
    const float L = (horiz ? lid.width : lid.height) / 2;
    const float Wd = (horiz ? lid.height : lid.width) / 2;
    auto pt = [&](float u, float v) { return horiz ? Vector2{c.x + u, c.y + v} : Vector2{c.x + v, c.y + u}; };
    // Lapis wrapping bands along the body.
    for (float u = -L * 0.1f; u < L - 6; u += 11)
    {
        const Vector2 a = pt(u, -Wd + 3), b = pt(u, Wd - 3);
        DrawLineEx(a, b, 3.0f, kLapis);
    }
    // Head end at the low side: mask with striped headdress.
    const Vector2 h = pt(-L + Wd + 2, 0);
    DrawCircleV(h, Wd, kLapis);
    DrawCircleV(h, Wd - 3, kGold);
    for (int k = -1; k <= 1; k += 2)
        DrawLineEx(pt(-L + Wd + 2 - Wd * 0.6f, k * Wd * 0.7f), pt(-L + Wd + 2 + Wd * 0.7f, k * Wd * 0.7f), 2.0f, kLapis);
    DrawCircleV(h, Wd * 0.5f, Color{226, 190, 140, 255});
    DrawCircleV(pt(-L + Wd + 2 - 2, -Wd * 0.18f), 1.4f, kOnyx);
    DrawCircleV(pt(-L + Wd + 2 - 2, Wd * 0.18f), 1.4f, kOnyx);
    // Crossed arms (a small X) on the chest.
    DrawLineEx(pt(-L * 0.35f - 4, -4), pt(-L * 0.35f + 4, 4), 2.0f, kTurquoise);
    DrawLineEx(pt(-L * 0.35f - 4, 4), pt(-L * 0.35f + 4, -4), 2.0f, kTurquoise);
}

void DrawPalm(const Piece& p)
{
    if (p.tw * p.th > 1)
    {
        BoxOutlined(Inset(p.r, 2), kSand, kSandDark, 0.08f, 3);
        DrawRectangleRec(Inset(p.r, 7), Color{96, 74, 50, 255});
    }
    for (int j = 0; j < p.th; ++j)
        for (int i = 0; i < p.tw; ++i)
        {
            const Vector2 c{p.r.x + i * TS + TS / 2.0f, p.r.y + j * TS + TS / 2.0f};
            if (p.tw * p.th == 1)
            {
                DrawCircleV(c, 14, Color{176, 104, 64, 255});
                DrawCircleV(c, 11, Color{92, 64, 44, 255});
            }
            for (int k = 0; k < 8; ++k)
            {
                const float a = k * PI / 4 + p.index * 0.4f + i + j;
                DrawLineEx(c, {c.x + std::cos(a) * 16, c.y + std::sin(a) * 16}, 6.0f, k % 2 ? Color{58, 126, 64, 255} : Color{88, 156, 74, 255});
                DrawLineEx(c, {c.x + std::cos(a) * 14, c.y + std::sin(a) * 14}, 1.5f, Color{40, 96, 48, 255});
            }
            DrawCircleV(c, 4, Color{110, 76, 44, 255});
        }
}

void DrawUrns(const Piece& p)
{
    for (int j = 0; j < p.th; ++j)
        for (int i = 0; i < p.tw; ++i)
        {
            const Rectangle t{p.r.x + i * TS + 2.0f, p.r.y + j * TS + 2.0f, TS - 4.0f, TS - 4.0f};
            const Vector2 c{t.x + t.width / 2, t.y + t.height / 2};
            switch (Hash(p.tx + i, p.ty + j, 61) % 3)
            {
            case 0: // clay urn
                DrawCircleV(c, 15, Color{150, 86, 54, 255});
                DrawRing(c, 10, 14, 0, 360, 24, Color{188, 112, 70, 255});
                DrawCircleV(c, 9, Color{40, 28, 24, 255});
                DrawRing(c, 14, 15, 0, 360, 24, kGold);
                break;
            case 1: // sandstone block
                BoxOutlined(t, kSand, kSandDark, 0.1f, 2);
                DrawLine(static_cast<int>(t.x), static_cast<int>(c.y), static_cast<int>(t.x + t.width), static_cast<int>(c.y), kSandDark);
                DrawLine(static_cast<int>(c.x), static_cast<int>(t.y), static_cast<int>(c.x), static_cast<int>(c.y), kSandDark);
                break;
            default: // canopic jar with a jackal lid
                DrawCircleV(c, 14, kSandDark);
                DrawCircleV(c, 11, kSand);
                DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y + 2), 4, 7, kOnyx);
                DrawCircleV({c.x - 5, c.y - 5}, 2.5f, kOnyx);
                DrawCircleV({c.x + 5, c.y - 5}, 2.5f, kOnyx);
                DrawRing(c, 11, 13, 0, 360, 24, kLapis);
                break;
            }
        }
}

void DrawBrazier(const Piece& p)
{
    BoxOutlined(Inset(p.r, 2), kSandDark, kOnyx, 0.12f, 2);
    for (int j = 0; j < p.th; ++j)
        for (int i = 0; i < p.tw; ++i)
        {
            const Vector2 c{p.r.x + i * TS + TS / 2.0f, p.r.y + j * TS + TS / 2.0f};
            DrawCircleV(c, 15, kGoldDark);
            DrawCircleV(c, 12, kOnyx);
            const float flick = 1.0f + 0.1f * std::sin(static_cast<float>(i * 3 + j));
            DrawCircleV(c, 9 * flick, Color{240, 110, 40, 255});
            DrawCircleV(c, 6, Color{255, 190, 70, 255});
            DrawCircleV(c, 3, Color{255, 245, 170, 255});
        }
}

void DrawReflectingPool(const Piece& p)
{
    const Rectangle r = Inset(p.r, 2);
    BoxOutlined(r, kSand, kSandDark, 0.12f, 2);
    const Rectangle w = Inset(r, 6);
    Box(w, Color{56, 144, 190, 255}, 0.2f);
    for (float y = w.y + 6; y < w.y + w.height - 3; y += 9)
        DrawLine(static_cast<int>(w.x + 6), static_cast<int>(y), static_cast<int>(w.x + w.width - 6), static_cast<int>(y), Color{110, 186, 220, 255});
    for (int k = 0; k < p.tw * p.th; ++k)
    {
        const int i = k % p.tw, j = k / p.tw;
        const Vector2 c{p.r.x + i * TS + 10 + Rand01(p.tx + i, p.ty + j, 71) * 20, p.r.y + j * TS + 10 + Rand01(p.tx + i, p.ty + j, 72) * 20};
        DrawCircleV(c, 6, Color{60, 130, 70, 255});
        DrawCircleV({c.x + 1, c.y - 1}, 3, Color{240, 140, 170, 255});
    }
}

void DrawStoneBench(const Piece& p)
{
    const Rectangle r = Inset(p.r, 3);
    BoxOutlined(r, kSand, kSandDark, 0.15f, 2);
    BoxOutlined(Inset(r, 5), kLapis, kGold, 0.2f, 2);
}

void DrawStatue(const Piece& p)
{
    const Rectangle r = Inset(p.r, 2);
    BoxOutlined(r, kSandDark, kOnyx, 0.1f, 2);
    Box(Inset(r, 4), kSand, 0.1f);
    if (p.tw * p.th == 1)
    {
        // Jackal-headed guardian bust.
        const Vector2 c{r.x + r.width / 2, r.y + r.height / 2 + 1};
        DrawCircleV(c, 11, kOnyx);
        DrawRing(c, 8, 11, 0, 360, 20, kGold);
        DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y + 3), 4, 8, Color{60, 56, 68, 255});
        FillPolygon({c.x - 5, c.y - 7}, {{c.x - 8, c.y - 3}, {c.x - 6, c.y - 15}, {c.x - 2, c.y - 5}}, kOnyx);
        FillPolygon({c.x + 5, c.y - 7}, {{c.x + 8, c.y - 3}, {c.x + 6, c.y - 15}, {c.x + 2, c.y - 5}}, kOnyx);
        DrawCircleV({c.x - 3, c.y - 2}, 1.4f, kGold);
        DrawCircleV({c.x + 3, c.y - 2}, 1.4f, kGold);
        return;
    }
    // Sphinx along the long axis, head at the high end.
    const bool horiz = Horizontal(p);
    const Vector2 c{r.x + r.width / 2, r.y + r.height / 2};
    const float L = (horiz ? r.width : r.height) / 2 - 4;
    const float Wd = (horiz ? r.height : r.width) / 2 - 4;
    auto pt = [&](float u, float v) { return horiz ? Vector2{c.x + u, c.y + v} : Vector2{c.x + v, c.y + u}; };
    Ellipse(pt(-L * 0.2f, 0), L * 0.62f, Wd * 0.72f, horiz, Color{214, 178, 122, 255});
    for (int k = -1; k <= 1; k += 2)
    {
        const Vector2 a = pt(L * 0.30f, k * Wd * 0.42f), b = pt(L * 0.86f, k * Wd * 0.42f);
        DrawLineEx(a, b, Wd * 0.34f, Color{226, 192, 138, 255});
    }
    DrawLineEx(pt(-L * 0.8f, 0), pt(-L * 0.98f, Wd * 0.45f), 2.0f, Color{170, 130, 84, 255});
    const Vector2 h = pt(L * 0.32f, 0);
    DrawCircleV(h, Wd * 0.62f, kLapis);
    DrawCircleV(h, Wd * 0.62f - 3, kGold);
    DrawCircleV(h, Wd * 0.36f, Color{226, 190, 140, 255});
}

void DrawTreasure(const Piece& p)
{
    for (int j = 0; j < p.th; ++j)
        for (int i = 0; i < p.tw; ++i)
        {
            const Rectangle t{p.r.x + i * TS + 2.0f, p.r.y + j * TS + 2.0f, TS - 4.0f, TS - 4.0f};
            const Vector2 c{t.x + t.width / 2, t.y + t.height / 2};
            if (Hash(p.tx + i, p.ty + j, 81) % 2 == 0)
            {
                // Chest
                BoxOutlined(t, Color{116, 72, 40, 255}, Color{60, 36, 22, 255}, 0.15f, 2);
                DrawRectangleRec({t.x + 2, c.y - 3, t.width - 4, 6}, kGold);
                DrawRectangleRec({c.x - 3, t.y + 3, 6, t.height - 6}, kGoldDark);
                DrawCircleV(c, 3.5f, kGold);
            }
            else
            {
                // Heap of coins and gems
                DrawCircleV({c.x, c.y + 2}, 15, kGoldDark);
                for (int k = 0; k < 9; ++k)
                {
                    const Vector2 q{c.x - 9 + Rand01(p.tx + i, p.ty + j, 90 + k) * 18, c.y - 9 + Rand01(p.tx + i, p.ty + j, 110 + k) * 18};
                    DrawCircleV(q, 4.5f, k % 2 ? kGold : Color{212, 160, 48, 255});
                }
                DrawCircleV({c.x + 4, c.y - 4}, 2.2f, Color{200, 50, 60, 255});
                DrawCircleV({c.x - 5, c.y + 3}, 2.2f, Color{60, 120, 220, 255});
            }
        }
}

// Returns false if this letter has no Egypt-specific art (falls back to the house furniture).
bool DrawEgyptPiece(const Piece& p, const ThemeStyle& st)
{
    switch (p.kind)
    {
    case 'B': DrawStele(p, st); return true;
    case 'T': DrawAltar(p); return true;
    case 'D': DrawSarcophagus(p); return true;
    case 'L': DrawPalm(p); return true;
    case 'X': DrawUrns(p); return true;
    case 'F': DrawBrazier(p); return true;
    case 'U': DrawReflectingPool(p); return true;
    case 'S': DrawStoneBench(p); return true;
    case 'M': DrawStatue(p); return true;
    case 'V': DrawTreasure(p); return true;
    case 'N':
        for (int j = 0; j < p.th; ++j)
            for (int i = 0; i < p.tw; ++i)
                DrawPillarTile({p.r.x + i * TS + TS / 2.0f, p.r.y + j * TS + TS / 2.0f}, Color{226, 216, 194, 255});
        return true;
    default: return false;
    }
}

void DrawPiece(const Level& level, const Piece& p, RoomTheme theme, const ThemeStyle& st)
{
    // Soft drop shadow first (pieces sit on the floor). Round tables and pianos draw their own.
    const bool roundTable = p.kind == 'T' && (theme == RoomTheme::Foyer || theme == RoomTheme::Parlor || theme == RoomTheme::Hallway) &&
                            !(p.tw * p.th == 1);
    const bool egypt = IsEgyptTheme(theme);
    const bool ownShadow = egypt && (p.kind == 'N' || p.kind == 'L' || p.kind == 'X' || p.kind == 'V');
    if (!roundTable && p.kind != 'Q' && !ownShadow)
        DrawRectangleRounded({p.r.x + 4, p.r.y + 5, p.r.width - 4, p.r.height - 4}, 0.15f, 6, Fade(BLACK, 0.25f));
    if (egypt && DrawEgyptPiece(p, st)) return;
    switch (p.kind)
    {
    case 'B': DrawShelf(p, theme, st); break;
    case 'T': DrawTable(p, theme, st); break;
    case 'K': DrawCounter(level, p, theme, st); break;
    case 'A': DrawAppliance(p, theme); break;
    case 'S': DrawSeat(level, p, theme, st); break;
    case 'D': DrawBed(level, p, theme, st); break;
    case 'G': DrawPoolTable(p, theme, st); break;
    case 'U': DrawTub(p, theme); break;
    case 'L': DrawPlant(p, theme); break;
    case 'X': DrawBoxes(p, theme); break;
    case 'Q': DrawPiano(p); break;
    case 'F': DrawFireplace(level, p); break;
    case 'W': DrawWardrobe(p, st); break;
    case 'Y': DrawCar(p); break;
    default: DrawRectangleRec(p.r, st.wood); break;
    }
}

// Split furniture into rectangles: from each unclaimed tile, grow right, then down while whole rows match.
std::vector<Piece> FindPieces(const Level& level)
{
    const int w = level.Width();
    const int h = level.Height();
    std::vector<bool> used(static_cast<size_t>(w) * h, false);
    std::vector<Piece> pieces;
    int counts[128] = {};
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const char k = level.FurnitureAt(x, y);
            if (!k || used[y * w + x]) continue;
            int pw = 1;
            while (x + pw < w && level.FurnitureAt(x + pw, y) == k && !used[y * w + x + pw]) ++pw;
            if (k == 'L' || k == 'X') // single plants / boxes stay separate unless they form a block
            {
                // keep growing normally; drawing handles per-tile detail
            }
            int ph = 1;
            while (y + ph < h)
            {
                bool full = true;
                for (int i = 0; i < pw; ++i)
                    if (level.FurnitureAt(x + i, y + ph) != k || used[(y + ph) * w + x + i]) full = false;
                if (!full) break;
                ++ph;
            }
            for (int j = 0; j < ph; ++j)
                for (int i = 0; i < pw; ++i) used[(y + j) * w + x + i] = true;
            pieces.push_back({k, x, y, pw, ph,
                              {static_cast<float>(x * TS), static_cast<float>(y * TS), static_cast<float>(pw * TS), static_cast<float>(ph * TS)},
                              counts[static_cast<int>(k)]++});
        }
    }
    return pieces;
}

void DrawWalls(const Level& level, const ThemeStyle& st)
{
    const int w = level.Width();
    const int h = level.Height();
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            if (level.At(x, y) != Tile::Wall) continue;
            const int px = x * TS;
            const int py = y * TS;
            DrawRectangle(px, py, TS, TS, st.wall);
            if (st.stone)
            {
                // Carved sandstone courses.
                DrawRectangle(px, py, TS, TS, Mix(st.wall, st.wallTop, Rand01(x, y, 31) * 0.3f));
                for (int k = 0; k < 2; ++k)
                {
                    DrawLine(px, py + k * 20, px + TS, py + k * 20, Shade(st.wall, 0.7f));
                    const int sx = px + (((x + y + k) % 2) ? 10 : 30);
                    DrawLine(sx, py + k * 20, sx, py + k * 20 + 20, Shade(st.wall, 0.7f));
                }
            }
            auto open = [&](int nx, int ny) { return nx >= 0 && ny >= 0 && nx < w && ny < h && level.At(nx, ny) != Tile::Wall; };
            // A light cap on edges that face into the room, a dark base line under it.
            if (open(x, y + 1)) { DrawRectangle(px, py + TS - 8, TS, 8, st.wallTop); DrawRectangle(px, py + TS - 2, TS, 2, Shade(st.wall, 0.6f)); }
            if (st.stone && open(x, y + 1))
            {
                DrawRectangle(px, py + TS - 12, TS, 3, st.accent);
                if (Hash(x, y, 50) % 3 != 0) DrawGlyph(px + 20.0f, py + 14.0f, static_cast<int>(Hash(x, y, 51)), Fade(st.accent, 0.9f));
            }
            if (open(x, y - 1)) DrawRectangle(px, py, TS, 4, st.wallTop);
            if (open(x - 1, y)) DrawRectangle(px, py, 4, TS, st.wallTop);
            if (open(x + 1, y)) DrawRectangle(px + TS - 4, py, 4, TS, st.wallTop);
        }
    }
}

void DrawWallShadows(const Level& level)
{
    for (int y = 0; y < level.Height(); ++y)
    {
        for (int x = 0; x < level.Width(); ++x)
        {
            if (level.IsWall(x, y)) continue;
            const int px = x * TS;
            const int py = y * TS;
            if (level.At(x, y - 1) == Tile::Wall) DrawRectangleGradientV(px, py, TS, 12, Fade(BLACK, 0.35f), Fade(BLACK, 0.0f));
            if (level.At(x - 1, y) == Tile::Wall) DrawRectangleGradientH(px, py, 8, TS, Fade(BLACK, 0.25f), Fade(BLACK, 0.0f));
        }
    }
}

void DrawHoleArt(const Level& level)
{
    for (const MouseHole& hole : level.Holes())
    {
        DrawCircleV(hole.center, 15.0f, Color{18, 14, 14, 255});
        DrawRing(hole.center, 14.0f, 18.0f, 0.0f, 360.0f, 32, HoleColor(hole.color));
    }
}
} // namespace

RoomRenderer::~RoomRenderer() { Release(); }

void RoomRenderer::Release()
{
    if (built_) UnloadRenderTexture(target_);
    built_ = false;
}

void RoomRenderer::Build(const Level& level)
{
    Release();
    const int w = level.Width() * TS;
    const int h = level.Height() * TS;
    target_ = LoadRenderTexture(w, h);
    built_ = true;

    const RoomTheme theme = level.Theme();
    const ThemeStyle& st = GetThemeStyle(theme);

    BeginTextureMode(target_);
    ClearBackground(st.wall);
    for (int y = 0; y < level.Height(); ++y)
        for (int x = 0; x < level.Width(); ++x)
            if (level.At(x, y) != Tile::Wall) DrawFloorTile(st, x, y);
    for (const Rug& rug : level.Rugs()) DrawRug(rug);
    DrawWallShadows(level);
    for (const Piece& p : FindPieces(level)) DrawPiece(level, p, theme, st);
    DrawWalls(level, st);
    DrawHoleArt(level);
    EndTextureMode();
}

void RoomRenderer::Draw(const Level& level, bool exitOpen) const
{
    if (built_)
    {
        // Render textures are stored upside down.
        const Rectangle src{0, 0, static_cast<float>(target_.texture.width), -static_cast<float>(target_.texture.height)};
        DrawTextureRec(target_.texture, src, {0, 0}, WHITE);
    }

    // The exit changes when all the cheese is found, so it's drawn live.
    for (int y = 0; y < level.Height(); ++y)
    {
        for (int x = 0; x < level.Width(); ++x)
        {
            if (level.At(x, y) != Tile::Exit) continue;
            const Vector2 c{(x + 0.5f) * TS, (y + 0.5f) * TS};
            const float pulse = exitOpen ? 2.0f * std::sin(static_cast<float>(GetTime()) * 4.0f) : 0.0f;
            DrawCircleV(c, 17.0f + pulse, exitOpen ? Fade(Color{60, 200, 110, 255}, 0.35f) : Fade(BLACK, 0.3f));
            DrawCircleV(c, 14.0f, exitOpen ? Color{60, 200, 110, 255} : Color{25, 25, 25, 255});
            if (!exitOpen) DrawRing(c, 14.0f, 17.0f, 0, 360, 32, Color{160, 60, 60, 255});
            DrawText("EXIT", static_cast<int>(c.x) - 12, static_cast<int>(c.y) - 5, 10, exitOpen ? Color{20, 60, 30, 255} : Color{160, 60, 60, 255});
        }
    }
}
