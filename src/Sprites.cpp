#include "Sprites.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "raymath.h"

namespace
{
// ------------------------------------------------------------------ helpers

// raylib only draws counter-clockwise triangles; accept any order.
void Tri(Vector2 a, Vector2 b, Vector2 c, Color col)
{
    const float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross > 0.0f) std::swap(b, c);
    DrawTriangle(a, b, c, col);
}

// Point in the sprite's local frame: `along` the facing direction, `side` to its left/right.
Vector2 At(Vector2 p, Vector2 f, Vector2 s, float along, float side)
{
    return {p.x + f.x * along + s.x * side, p.y + f.y * along + s.y * side};
}

Vector2 Rotate(Vector2 v, float a)
{
    const float c = std::cos(a), sn = std::sin(a);
    return {v.x * c - v.y * sn, v.x * sn + v.y * c};
}

// An ellipse rotated to the facing direction (rx along f, ry across).
void FillOval(Vector2 c, Vector2 f, Vector2 s, float rx, float ry, Color col, int segments = 24)
{
    Vector2 prev = At(c, f, s, rx, 0);
    for (int i = 1; i <= segments; ++i)
    {
        const float a = i * 2.0f * PI / segments;
        const Vector2 p = At(c, f, s, std::cos(a) * rx, std::sin(a) * ry);
        Tri(c, prev, p, col);
        prev = p;
    }
}

// A tapering curve drawn as overlapping round segments.
void Strand(const std::vector<Vector2>& pts, float w0, float w1, Color a, Color b = BLANK, int bandEvery = 0)
{
    for (size_t i = 1; i < pts.size(); ++i)
    {
        const float u = static_cast<float>(i) / (pts.size() - 1);
        const float w = w0 + (w1 - w0) * u;
        const Color col = (bandEvery > 0 && (i / bandEvery) % 2 == 1) ? b : a;
        DrawLineEx(pts[i - 1], pts[i], w, col);
        DrawCircleV(pts[i], w / 2, col);
    }
}

Color Mix(Color a, Color b, float t)
{
    auto ch = [t](unsigned char x, unsigned char y) { return static_cast<unsigned char>(x + (y - x) * t); };
    return {ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b), a.a};
}

const Color kPink{238, 150, 165, 255};
const Color kPinkDark{200, 105, 125, 255};
} // namespace

// ------------------------------------------------------------------ mouse

void DrawMouseSprite(const MouseLook& m)
{
    const float k = m.scale;
    const Vector2 f = m.facing;
    const Vector2 s{-f.y, f.x};
    const Vector2 p = m.pos;
    const float t = static_cast<float>(GetTime());

    const bool aviator = m.skin == MouseSkin::Aviator;
    // Aviator: a slate-blue coat instead of the classic grey.
    const Color body = m.flashRed ? Color{232, 70, 58, 255} : (aviator ? Color{116, 140, 178, 255} : Color{172, 172, 186, 255});
    const Color light = m.flashRed ? Color{255, 130, 115, 255} : (aviator ? Color{164, 186, 216, 255} : Color{210, 210, 222, 255});
    const Color dark = m.flashRed ? Color{170, 40, 36, 255} : (aviator ? Color{80, 98, 134, 255} : Color{130, 130, 146, 255});

    // Shadow
    FillOval({p.x + 2 * k, p.y + 3 * k}, f, s, 15 * k, 11 * k, Fade(BLACK, 0.25f));

    // Tail: a long pink curve that sways (more when running).
    std::vector<Vector2> tail;
    for (int i = 0; i <= 14; ++i)
    {
        const float u = i / 14.0f;
        const float wave = std::sin(t * (m.moving ? 10.0f : 3.0f) - u * 4.0f) * (m.moving ? 5.0f : 3.0f) * u;
        tail.push_back(At(p, f, s, (-11.0f - u * 26.0f) * k, wave * k));
    }
    Strand(tail, 3.2f * k, 1.0f * k, kPink);

    // Feet step while walking.
    const float step = m.moving ? std::sin(m.walkPhase) * 3.0f : 0.0f;
    DrawCircleV(At(p, f, s, (7 + step) * k, 8.5f * k), 2.7f * k, kPink);
    DrawCircleV(At(p, f, s, (7 - step) * k, -8.5f * k), 2.7f * k, kPink);
    DrawCircleV(At(p, f, s, (-7 - step) * k, 8.5f * k), 3.0f * k, kPink);
    DrawCircleV(At(p, f, s, (-7 + step) * k, -8.5f * k), 3.0f * k, kPink);

    // Body with a soft darker rim and a highlight along the back.
    FillOval(At(p, f, s, -2 * k, 0), f, s, 13.5f * k, 10.5f * k, dark);
    FillOval(At(p, f, s, -2 * k, 0), f, s, 12.5f * k, 9.5f * k, body);
    FillOval(At(p, f, s, -3 * k, -1.5f * k), f, s, 8.5f * k, 5.0f * k, light);

    // Ears: big and round, pink inside.
    for (float sg : {1.0f, -1.0f})
    {
        const Vector2 e = At(p, f, s, 6 * k, sg * 8.5f * k);
        DrawCircleV(e, 6.5f * k, dark);
        DrawCircleV(e, 5.5f * k, body);
        DrawCircleV(At(e, f, s, 0.8f * k, 0), 3.6f * k, kPink);
    }

    // Aviator scarf: a red band round the neck with two ends streaming out behind.
    if (aviator)
    {
        const Color red{206, 52, 62, 255}, redDark{140, 30, 42, 255}, cream{246, 226, 196, 255};
        const float speed = m.moving ? 11.0f : 3.5f;
        const float amp = m.moving ? 5.5f : 2.0f;
        const float len = m.moving ? 21.0f : 14.0f;
        for (int end = 0; end < 2; ++end)
        {
            const float sg = end == 0 ? 1.0f : -1.0f;
            std::vector<Vector2> pts;
            for (int i = 0; i <= 10; ++i)
            {
                const float u = i / 10.0f;
                const float wave = std::sin(t * speed - u * 4.5f + end * 1.7f) * amp * u;
                pts.push_back(At(p, f, s, (2.5f - u * len) * k, (sg * (3.0f + u * 2.0f) + wave) * k));
            }
            Strand(pts, 6.0f * k, 3.6f * k, redDark);
            Strand(pts, 4.6f * k, 2.4f * k, red);
            DrawCircleV(pts.back(), 1.6f * k, cream); // fringe
        }
        FillOval(At(p, f, s, 3.0f * k, 0), f, s, 3.4f * k, 9.6f * k, redDark);
        FillOval(At(p, f, s, 3.3f * k, 0), f, s, 2.6f * k, 8.6f * k, red);
    }

    // Head and snout
    DrawCircleV(At(p, f, s, 9 * k, 0), 8.0f * k, body);
    FillOval(At(p, f, s, 14 * k, 0), f, s, 6.5f * k, 4.6f * k, light);

    // Aviator cap: brown leather with a seam, worn over the back of the head (the ears stay out).
    if (aviator)
    {
        const Color leather{128, 80, 46, 255}, leatherLight{166, 108, 64, 255}, leatherDark{86, 52, 30, 255};
        DrawCircleV(At(p, f, s, 6.4f * k, 0), 6.6f * k, leatherDark);
        DrawCircleV(At(p, f, s, 6.2f * k, 0), 5.8f * k, leather);
        FillOval(At(p, f, s, 5.0f * k, -1.4f * k), f, s, 3.4f * k, 2.4f * k, leatherLight);
        DrawLineEx(At(p, f, s, 1.2f * k, 0), At(p, f, s, 11.4f * k, 0), 0.8f * k, leatherDark);
        // Goggle strap round the head.
        DrawLineEx(At(p, f, s, 10.0f * k, -7.6f * k), At(p, f, s, 10.0f * k, 7.6f * k), 1.6f * k, Color{48, 34, 28, 255});
    }

    // Eyes with a glint
    for (float sg : {1.0f, -1.0f})
    {
        const Vector2 e = At(p, f, s, 12 * k, sg * 3.9f * k);
        if (aviator)
        {
            // Round brass goggles with cyan lenses; the eye peeks through.
            DrawCircleV(e, 3.8f * k, Color{176, 136, 48, 255});
            DrawCircleV(e, 3.0f * k, Color{96, 200, 224, 255});
            DrawCircleV(At(e, f, s, 0.4f * k, 0), 1.6f * k, Color{24, 22, 30, 255});
            DrawCircleV(At(e, f, s, -0.8f * k, -1.2f * k), 0.9f * k, Fade(WHITE, 0.85f));
            continue;
        }
        DrawCircleV(e, 2.0f * k, Color{24, 22, 30, 255});
        DrawCircleV(At(e, f, s, 0.5f * k, -0.6f * k), 0.75f * k, WHITE);
    }
    if (aviator) DrawLineEx(At(p, f, s, 12.0f * k, -1.2f * k), At(p, f, s, 12.0f * k, 1.2f * k), 1.4f * k, Color{176, 136, 48, 255}); // goggle bridge

    // Nose and whiskers
    DrawCircleV(At(p, f, s, 19.8f * k, 0), 2.3f * k, kPinkDark);
    for (float sg : {1.0f, -1.0f})
        for (int j = -1; j <= 1; ++j)
            DrawLineEx(At(p, f, s, 17 * k, sg * 2.0f * k), At(p, f, s, (18.0f + j * 3.5f) * k, sg * 11.0f * k), 0.8f * k, Fade(WHITE, 0.65f));
}

// ------------------------------------------------------------------ cats

void DrawCatSprite(const CatLook& c)
{
    const CatStats& st = GetCatStats(c.kind);
    const float k = c.scale;
    const Vector2 f = c.facing;
    const Vector2 s{-f.y, f.x};
    const Vector2 p = c.pos;
    const float t = static_cast<float>(GetTime()) + c.tailSeed;

    Color fur = st.fur;
    Color dark = st.furDark;
    const bool siamese = c.kind == CatKind::Hunter;
    const bool boss = c.kind == CatKind::Boss;
    const Color tuxWhite{236, 234, 228, 255};
    const Color points = siamese ? Color{78, 58, 48, 255} : dark; // Siamese: dark ears, face, paws, tail
    const Color innerEar{222, 150, 150, 255};
    const Color belly = Mix(fur, WHITE, 0.3f);

    // ---------------------------------------------------------- asleep: curled into a ball
    if (c.pose == CatPose::Asleep)
    {
        FillOval({p.x + 2 * k, p.y + 3 * k}, f, s, 16 * k, 16 * k, Fade(BLACK, 0.25f));
        // Tail wrapped around the body
        std::vector<Vector2> tail;
        for (int i = 0; i <= 16; ++i)
        {
            const float a = PI * 0.9f + i / 16.0f * PI * 1.3f;
            tail.push_back(At(p, f, s, std::cos(a) * 15 * k, std::sin(a) * 15 * k));
        }
        DrawCircleV(p, 15 * k, Mix(fur, BLACK, 0.2f));
        DrawCircleV(p, 14 * k, fur);
        if (c.kind == CatKind::Tabby)
            for (int i = 0; i < 4; ++i)
            {
                const float a = -0.6f + i * 0.5f;
                DrawLineEx(At(p, f, s, std::cos(a) * 5 * k, std::sin(a) * 5 * k), At(p, f, s, std::cos(a) * 13 * k, std::sin(a) * 13 * k), 2.5f * k, Fade(dark, 0.8f));
            }
        Strand(tail, 5 * k, 3 * k, siamese ? points : fur, dark, c.kind == CatKind::Tabby ? 3 : 0);
        // Head tucked in on one side
        const Vector2 hp = At(p, f, s, 5 * k, 4 * k);
        for (float sg : {1.0f, -1.0f})
        {
            const Vector2 base1 = Vector2Add(hp, Vector2Scale(Rotate(f, sg * 0.5f), 8 * k));
            const Vector2 base2 = Vector2Add(hp, Vector2Scale(Rotate(f, sg * 1.3f), 8 * k));
            const Vector2 tip = Vector2Add(hp, Vector2Scale(Rotate(f, sg * 0.9f), 13 * k));
            Tri(base1, base2, tip, points);
        }
        DrawCircleV(hp, 8.5f * k, siamese ? Mix(fur, points, 0.4f) : fur);
        for (float sg : {1.0f, -1.0f})
        {
            const Vector2 e = At(hp, f, s, 2.5f * k, sg * 3.5f * k);
            DrawLineEx(At(e, f, s, 0, -1.8f * k), At(e, f, s, 0.8f * k, 1.8f * k), 1.3f * k, Mix(dark, BLACK, 0.4f));
        }
        DrawCircleV(At(hp, f, s, 6 * k, 0), 1.6f * k, kPinkDark);
        return;
    }

    // ---------------------------------------------------------- standing / walking
    const float stretch = c.pose == CatPose::Lunge ? 1.3f : (c.pose == CatPose::WindUp ? 0.88f : 1.0f);
    const float squash = c.pose == CatPose::WindUp ? 1.1f : (c.pose == CatPose::Lunge ? 0.85f : 1.0f);

    FillOval(At(p, f, s, (-3 * k) + 2 * k, 3 * k), f, s, 19 * k * stretch, 13 * k, Fade(BLACK, 0.25f));

    // Tail: swishes lazily on patrol, lashes when hunting or about to pounce.
    const float swishSpeed = c.pose == CatPose::WindUp ? 16.0f : (c.alert ? 7.0f : 2.2f);
    const float swishAmp = c.pose == CatPose::Lunge ? 2.0f : 7.0f;
    std::vector<Vector2> tail;
    for (int i = 0; i <= 14; ++i)
    {
        const float u = i / 14.0f;
        const float wave = std::sin(t * swishSpeed - u * 3.0f) * swishAmp * u + (c.pose == CatPose::Stunned ? 6 * u : 0);
        tail.push_back(At(p, f, s, (-17.0f * stretch - u * 30.0f) * k, wave * k));
    }
    const Color tailColor = siamese ? points : fur;
    Strand(tail, 6 * k, 3 * k, Mix(tailColor, BLACK, 0.2f));
    Strand(tail, 4.5f * k, 2.2f * k, tailColor, dark, c.kind == CatKind::Tabby ? 3 : 0);

    // Paws
    const float step = c.moving ? std::sin(c.walkPhase) * 4.0f : 0.0f;
    const Color paw = siamese ? points : (boss ? tuxWhite : Mix(fur, WHITE, 0.2f));
    const float px = 10 * k * squash;
    DrawCircleV(At(p, f, s, (8 + step) * k * stretch, px), 3.6f * k, paw);
    DrawCircleV(At(p, f, s, (8 - step) * k * stretch, -px), 3.6f * k, paw);
    DrawCircleV(At(p, f, s, (-12 - step) * k * stretch, px), 3.8f * k, paw);
    DrawCircleV(At(p, f, s, (-12 + step) * k * stretch, -px), 3.8f * k, paw);

    // Body
    const Vector2 bc = At(p, f, s, -4 * k * stretch, 0);
    FillOval(bc, f, s, 18 * k * stretch, 12.5f * k * squash, Mix(fur, BLACK, 0.22f));
    FillOval(bc, f, s, 17 * k * stretch, 11.5f * k * squash, fur);

    // Coat pattern
    switch (c.kind)
    {
    case CatKind::Tabby:
        for (int i = -2; i <= 2; ++i)
        {
            const float along = -4.0f + i * 6.0f;
            const float half = 9.0f * std::sqrt(std::max(0.0f, 1.0f - (i * 6.0f / 17.0f) * (i * 6.0f / 17.0f)));
            DrawLineEx(At(p, f, s, along * k * stretch, -half * k * squash), At(p, f, s, (along + 1.5f) * k * stretch, half * k * squash), 2.6f * k, Fade(dark, 0.85f));
        }
        break;
    case CatKind::Hunter: // darker toward the hindquarters
        FillOval(At(p, f, s, -14 * k * stretch, 0), f, s, 7 * k, 8 * k * squash, Mix(fur, points, 0.35f));
        break;
    case CatKind::Sleepy: // fluffy outline
        for (int i = 0; i < 12; ++i)
        {
            const float a = i * 2.0f * PI / 12.0f;
            DrawCircleV(At(bc, f, s, std::cos(a) * 16 * k * stretch, std::sin(a) * 10.5f * k * squash), 3.4f * k, fur);
        }
        break;
    case CatKind::Blind: // a lighter patch
        FillOval(At(p, f, s, -7 * k * stretch, 3 * k), f, s, 7 * k, 5 * k, Mix(fur, WHITE, 0.2f));
        break;
    case CatKind::Boss: // tuxedo: white chest, plus a red collar with a gold bell
        FillOval(At(p, f, s, 4 * k * stretch, 0), f, s, 8 * k * stretch, 7 * k * squash, tuxWhite);
        DrawLineEx(At(p, f, s, 8 * k * stretch, -8.5f * k), At(p, f, s, 8 * k * stretch, 8.5f * k), 3.2f * k, Color{190, 30, 40, 255});
        DrawCircleV(At(p, f, s, 9.5f * k * stretch, 0), 2.4f * k, GOLD);
        DrawCircleV(At(p, f, s, 9.5f * k * stretch, -0.6f * k), 0.8f * k, Color{255, 240, 170, 255});
        break;
    }
    FillOval(At(p, f, s, -5 * k * stretch, -3.5f * k), f, s, 10 * k * stretch, 4 * k, Fade(WHITE, 0.08f)); // sheen

    // Head, with pointed ears sitting on top toward the back of the head.
    const Vector2 hp = At(p, f, s, 12 * k * stretch, 0);
    DrawCircleV(hp, 11.0f * k, Mix(fur, BLACK, 0.22f));
    DrawCircleV(hp, 10.2f * k, fur);
    const Color ear = siamese ? points : (c.kind == CatKind::Tabby ? dark : Mix(fur, BLACK, 0.15f));
    const float earSpread = c.pose == CatPose::WindUp ? 0.25f : 0.0f; // ears flatten back when about to pounce
    for (float sg : {1.0f, -1.0f})
    {
        // Base on the side/back of the head (behind the eyes), tip pointing outward and a little forward.
        const Vector2 dir = Rotate(f, sg * (1.45f + earSpread));
        const Vector2 base1 = Vector2Add(hp, Vector2Scale(Rotate(f, sg * (1.15f + earSpread)), 8.0f * k));
        const Vector2 base2 = Vector2Add(hp, Vector2Scale(Rotate(f, sg * (2.25f + earSpread)), 8.0f * k));
        const Vector2 tip = Vector2Add(hp, Vector2Scale(dir, 17.0f * k));
        Tri(Vector2Add(base1, Vector2Scale(dir, -1.0f * k)), Vector2Add(base2, Vector2Scale(dir, -1.0f * k)),
            Vector2Add(tip, Vector2Scale(dir, 1.3f * k)), Mix(ear, BLACK, 0.3f)); // outline
        Tri(base1, base2, tip, ear);
        const Vector2 ib1 = Vector2Add(hp, Vector2Scale(Rotate(f, sg * (1.35f + earSpread)), 9.5f * k));
        const Vector2 ib2 = Vector2Add(hp, Vector2Scale(Rotate(f, sg * (1.95f + earSpread)), 9.5f * k));
        const Vector2 itip = Vector2Add(hp, Vector2Scale(dir, 14.5f * k));
        Tri(ib1, ib2, itip, siamese ? Color{150, 110, 100, 255} : innerEar);
    }
    if (siamese) FillOval(At(hp, f, s, 4 * k, 0), f, s, 6.5f * k, 6.5f * k, points); // dark face mask
    if (c.kind == CatKind::Tabby) // "M" forehead stripes
        for (int j = -1; j <= 1; ++j)
            DrawLineEx(At(hp, f, s, -3 * k, j * 3.0f * k), At(hp, f, s, 2 * k, j * 2.0f * k), 1.6f * k, Fade(dark, 0.9f));
    FillOval(At(hp, f, s, 6.5f * k, 0), f, s, 4.2f * k, 5.2f * k, siamese ? Mix(points, fur, 0.25f) : (boss ? tuxWhite : belly)); // muzzle
    if (boss) // battle scar over one eye
    {
        DrawLineEx(At(hp, f, s, 7 * k, -2.5f * k), At(hp, f, s, 0.5f * k, -7.5f * k), 1.3f * k, Color{170, 90, 100, 255});
        for (int i = 0; i < 3; ++i)
        {
            const Vector2 m = At(hp, f, s, (5.5f - i * 2.2f) * k, (-3.6f - i * 1.6f) * k);
            DrawLineEx(At(m, f, s, 1.0f * k, 1.0f * k), At(m, f, s, -1.0f * k, -1.0f * k), 1.0f * k, Color{170, 90, 100, 255});
        }
    }

    // Eyes
    for (float sg : {1.0f, -1.0f})
    {
        const Vector2 e = At(hp, f, s, 3.2f * k, sg * 4.8f * k);
        if (c.pose == CatPose::Stunned)
        {
            DrawLineEx({e.x - 2.5f * k, e.y - 2.5f * k}, {e.x + 2.5f * k, e.y + 2.5f * k}, 1.6f * k, Color{30, 26, 30, 255});
            DrawLineEx({e.x - 2.5f * k, e.y + 2.5f * k}, {e.x + 2.5f * k, e.y - 2.5f * k}, 1.6f * k, Color{30, 26, 30, 255});
            continue;
        }
        if (c.kind == CatKind::Blind)
        {
            DrawCircleV(e, 3.2f * k, Color{214, 220, 232, 255});
            DrawCircleV(e, 1.8f * k, Fade(Color{160, 180, 210, 255}, 0.6f));
            continue;
        }
        DrawCircleV(e, 3.3f * k, Color{20, 18, 20, 255});
        DrawCircleV(e, 2.8f * k, st.eye);
        const bool wide = c.alert || c.pose == CatPose::WindUp || c.pose == CatPose::Lunge;
        if (wide)
            DrawCircleV(e, 2.0f * k, Color{15, 12, 15, 255});
        else
            DrawLineEx(At(e, f, s, -2.2f * k, 0), At(e, f, s, 2.2f * k, 0), 1.2f * k, Color{15, 12, 15, 255}); // slit
        DrawCircleV(At(e, f, s, 0.9f * k, -0.9f * k), 0.8f * k, WHITE);
    }

    // Nose and whiskers
    Tri(At(hp, f, s, 10.2f * k, 0), At(hp, f, s, 8.2f * k, 1.9f * k), At(hp, f, s, 8.2f * k, -1.9f * k), kPinkDark);
    for (float sg : {1.0f, -1.0f})
        for (int j = -1; j <= 1; ++j)
            DrawLineEx(At(hp, f, s, 8 * k, sg * 3 * k), At(hp, f, s, (11.0f + j * 3.5f) * k, sg * 14.0f * k), 0.9f * k, Fade(WHITE, 0.6f));

    if (boss && c.pose != CatPose::Asleep)
    {
        // A little golden crown, drawn upright so it always reads as a crown.
        const Vector2 cc = At(hp, f, s, -3 * k, 0);
        const float w = 9 * k, h = 6 * k;
        const Color gold{240, 196, 60, 255};
        const Color goldDark{180, 130, 30, 255};
        const float tilt = c.pose == CatPose::Stunned ? 0.35f : 0.0f; // knocked askew when dazed
        auto pt = [&](float x, float y) { return Vector2Add(cc, Rotate({x, y}, tilt)); };
        Tri(pt(-w, 0), pt(w, 0), pt(w, -h * 0.5f), goldDark);
        Tri(pt(-w, 0), pt(w, -h * 0.5f), pt(-w, -h * 0.5f), goldDark);
        Tri(pt(-w, -h * 0.4f), pt(w, -h * 0.4f), pt(w, -h * 0.9f), gold);
        Tri(pt(-w, -h * 0.4f), pt(w, -h * 0.9f), pt(-w, -h * 0.9f), gold);
        for (float x : {-w, 0.0f, w})
            Tri(pt(x - 3 * k, -h * 0.85f), pt(x + 3 * k, -h * 0.85f), pt(x, -h * 1.8f), gold);
        DrawCircleV(pt(0, -h * 0.65f), 1.6f * k, Color{200, 40, 60, 255});
        DrawCircleV(pt(-w * 0.55f, -h * 0.65f), 1.1f * k, Color{60, 120, 220, 255});
        DrawCircleV(pt(w * 0.55f, -h * 0.65f), 1.1f * k, Color{60, 120, 220, 255});
    }
}

// ------------------------------------------------------------------ cheese

void DrawCheeseSprite(CheeseKind kind, Vector2 c, float rotation, float k)
{
    const Vector2 f{std::cos(rotation), std::sin(rotation)};
    const Vector2 s{-f.y, f.x};
    FillOval({c.x + 2 * k, c.y + 3 * k}, f, s, 11 * k, 9 * k, Fade(BLACK, 0.28f));

    if (kind == CheeseKind::Gouda)
    {
        // A whole wax-covered wheel with a slice cut out.
        const Color wax{186, 36, 40, 255};
        DrawCircleV(c, 13 * k, Color{120, 20, 24, 255});
        DrawCircleV(c, 12 * k, wax);
        DrawRing(c, 8 * k, 10 * k, -140, -40, 16, Fade(WHITE, 0.18f));
        Tri(c, At(c, f, s, 13 * k, 5.5f * k), At(c, f, s, 13 * k, -5.5f * k), Color{244, 204, 96, 255});
        Tri(c, At(c, f, s, 13 * k, 5.5f * k), At(c, f, s, 7 * k, 0), Color{226, 184, 80, 255});
        DrawCircleV(At(c, f, s, -4 * k, 4 * k), 2.5f * k, Fade(Color{255, 230, 150, 255}, 0.6f)); // label
        return;
    }

    if (kind == CheeseKind::Golden)
    {
        // Shining golden wedge with twinkling sparkles.
        const float t = static_cast<float>(GetTime());
        DrawCircleV(c, 20 * k, Fade(Color{255, 220, 90, 255}, 0.18f + 0.1f * std::sin(t * 4)));
        const Vector2 tip = At(c, f, s, 14 * k, 0), b1 = At(c, f, s, -10 * k, 11 * k), b2 = At(c, f, s, -10 * k, -11 * k);
        Tri(At(tip, f, s, 1.5f * k, 0), At(b1, f, s, -1.5f * k, 1.5f * k), At(b2, f, s, -1.5f * k, -1.5f * k), Color{170, 120, 20, 255});
        Tri(tip, b1, b2, Color{255, 204, 50, 255});
        DrawLineEx(b1, b2, 4 * k, Color{220, 160, 30, 255});
        Tri(At(c, f, s, 9 * k, 0), At(c, f, s, -3 * k, 3 * k), At(c, f, s, -3 * k, -1 * k), Fade(WHITE, 0.45f));
        for (int i = 0; i < 4; ++i)
        {
            const float a = t * 1.5f + i * PI / 2;
            const float r = (14 + 3 * std::sin(t * 5 + i)) * k;
            const Vector2 sp{c.x + std::cos(a) * r, c.y + std::sin(a) * r};
            const float sz = (2.5f + 1.5f * std::sin(t * 6 + i * 1.7f)) * k;
            DrawLineEx({sp.x - sz, sp.y}, {sp.x + sz, sp.y}, 1.2f * k, WHITE);
            DrawLineEx({sp.x, sp.y - sz}, {sp.x, sp.y + sz}, 1.2f * k, WHITE);
        }
        return;
    }

    // Wedge: tip forward, rind along the back edge.
    const Vector2 tip = At(c, f, s, 12 * k, 0);
    const Vector2 b1 = At(c, f, s, -9 * k, 9.5f * k);
    const Vector2 b2 = At(c, f, s, -9 * k, -9.5f * k);
    Color face{}, rind{}, edge{};
    switch (kind)
    {
    case CheeseKind::Cheddar: face = {246, 168, 44, 255}; rind = {206, 116, 28, 255}; edge = {190, 110, 30, 255}; break;
    case CheeseKind::Swiss:   face = {250, 222, 110, 255}; rind = {224, 186, 70, 255}; edge = {200, 164, 60, 255}; break;
    case CheeseKind::Brie:    face = {246, 232, 186, 255}; rind = {250, 248, 240, 255}; edge = {214, 206, 180, 255}; break;
    case CheeseKind::Blue:    face = {236, 234, 218, 255}; rind = {206, 200, 172, 255}; edge = {186, 180, 156, 255}; break;
    default: break;
    }
    Tri(At(tip, f, s, 1 * k, 0), At(b1, f, s, -1 * k, 1 * k), At(b2, f, s, -1 * k, -1 * k), edge); // outline
    Tri(tip, b1, b2, face);
    if (kind == CheeseKind::Brie)
    {
        // Brie has a soft white rind all around.
        Tri(tip, b1, b2, rind);
        Tri(At(c, f, s, 8.5f * k, 0), At(c, f, s, -6.5f * k, 6.5f * k), At(c, f, s, -6.5f * k, -6.5f * k), face);
    }
    else
    {
        DrawLineEx(b1, b2, 3.6f * k, rind);
    }
    // Highlight on the top face
    Tri(At(c, f, s, 7 * k, 0), At(c, f, s, -2 * k, 2.5f * k), At(c, f, s, -2 * k, -1.5f * k), Fade(WHITE, 0.22f));

    if (kind == CheeseKind::Swiss)
    {
        const Color hole{214, 176, 64, 255};
        DrawCircleV(At(c, f, s, -3 * k, 3.5f * k), 2.3f * k, hole);
        DrawCircleV(At(c, f, s, 3 * k, -1.5f * k), 1.7f * k, hole);
        DrawCircleV(At(c, f, s, -5 * k, -4 * k), 1.9f * k, hole);
        DrawCircleV(At(c, f, s, 7 * k, 0.5f * k), 1.1f * k, hole);
    }
    if (kind == CheeseKind::Blue)
    {
        const Color vein{88, 110, 150, 255};
        DrawLineEx(At(c, f, s, -5 * k, -5 * k), At(c, f, s, 1 * k, -1 * k), 1.2f * k, vein);
        DrawLineEx(At(c, f, s, 1 * k, -1 * k), At(c, f, s, 4 * k, 0.5f * k), 1.0f * k, vein);
        DrawLineEx(At(c, f, s, -6 * k, 4 * k), At(c, f, s, -2 * k, 1.5f * k), 1.1f * k, vein);
        DrawCircleV(At(c, f, s, -1 * k, 4 * k), 1.2f * k, vein);
        DrawCircleV(At(c, f, s, -6 * k, 0), 1.0f * k, vein);
        DrawCircleV(At(c, f, s, 5 * k, -1.5f * k), 0.9f * k, vein);
    }
}
