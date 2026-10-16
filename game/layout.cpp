#include "layout.hpp"

namespace kid {

MaterialProps material_props(Material m) {
    switch (m) {
        case Material::Wood: return {1.0f, 0.5f, 0.1f};
        case Material::Stone: return {3.0f, 0.7f, 0.05f};
        case Material::Ice: return {0.9f, 0.04f, 0.1f};
    }
    return {1.0f, 0.5f, 0.1f};
}

AmmoProps ammo_props(Ammo a) {
    switch (a) {
        case Ammo::Ball: return {"ball", 0.25f, 6.0f, 0.2f, 0.5f};
        case Ammo::Heavy: return {"heavy", 0.33f, 18.0f, 0.1f, 0.5f};
        case Ammo::Blast: return {"blast", 0.25f, 6.0f, 0.2f, 0.5f};
    }
    return {"ball", 0.25f, 6.0f, 0.2f, 0.5f};
}

void wall(Layout& l, Vec2 pos, Vec2 half, Real angle) { l.walls.push_back({pos, half, angle}); }

Real platform(Layout& l, Real x, Real top, Real half_width, bool pillar) {
    constexpr Real kSlab = 0.2f;
    wall(l, {x, top - kSlab}, {half_width, kSlab});
    const Real pillar_half = (top - 2 * kSlab - kFloorTop) * 0.5f;
    if (pillar && pillar_half > 0) wall(l, {x, kFloorTop + pillar_half}, {0.2f, pillar_half});
    const Real line = top - 0.1f;
    l.fall_lines.push_back({line, x - half_width - 1.0f, x + half_width + 1.0f});
    return line;
}

int piece(Layout& l, Kind kind, Material m, Vec2 pos, Vec2 half, Real fall_y, Real angle) {
    Piece p;
    p.kind = kind;
    p.material = m;
    p.pos = pos;
    p.half = half;
    p.angle = angle;
    p.fall_y = fall_y;
    l.pieces.push_back(p);
    return static_cast<int>(l.pieces.size()) - 1;
}

int disc(Layout& l, Kind kind, Material m, Vec2 pos, Real radius, Real fall_y) {
    const int i = piece(l, kind, m, pos, {radius, radius}, fall_y);
    l.pieces[static_cast<size_t>(i)].radius = radius;
    return i;
}

int pyramid(Layout& l, Kind kind, Material m, Real x, Real base, int rows, Real half, Real fall_y) {
    // Blocks a few centimetres apart in a row, and rows a millimetre apart, so nothing starts out overlapping.
    const Real pitch = 2 * half + 0.05f;
    const int first = static_cast<int>(l.pieces.size());
    for (int row = 0; row < rows; ++row)
        for (int i = 0; i < rows - row; ++i) {
            const Real px = x + (static_cast<Real>(i) - static_cast<Real>(rows - row - 1) * 0.5f) * pitch;
            const Real py = base + half + static_cast<Real>(row) * (2 * half + 0.001f);
            piece(l, kind, m, {px, py}, {half, half}, fall_y);
        }
    return first;
}

int column(Layout& l, Kind kind, Material m, Real x, Real base, int count, Vec2 half, Real fall_y) {
    const int first = static_cast<int>(l.pieces.size());
    for (int i = 0; i < count; ++i)
        piece(l, kind, m, {x, base + half.y + static_cast<Real>(i) * (2 * half.y + 0.001f)}, half, fall_y);
    return first;
}

}  // namespace kid
