#include "layout.hpp"

namespace kid {

MaterialProps material_props(Material m) {
    // A standard 0.7 m block has 0.49 m^2: about 2 N s of health in ice, 5 in wood, 15 in stone. A
    // ball at full speed hits with 5 to 10 N s; a block dropped its own height lands with about 2.
    switch (m) {
        case Material::Wood: return {1.0f, 0.5f, 0.1f, 10.0f, 500};
        case Material::Stone: return {3.0f, 0.7f, 0.05f, 30.0f, 800};
        case Material::Ice: return {0.9f, 0.04f, 0.1f, 4.0f, 300};
    }
    return {1.0f, 0.5f, 0.1f, 10.0f, 500};
}

PigProps pig_props(Pig p) {
    // A small pig pops from a fall of about a metre; a big one needs more than any single ball can hit it with.
    switch (p) {
        case Pig::Small: return {"pig", 0.3f, 1.5f, 5000};
        case Pig::Helmet: return {"helmet pig", 0.35f, 5.0f, 5000};
        case Pig::Big: return {"big pig", 0.5f, 12.0f, 10000};  // more than one ball can deliver
    }
    return {"pig", 0.3f, 1.5f, 5000};
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

void platform(Layout& l, Real x, Real top, Real half_width) {
    constexpr Real kSlab = 0.2f;
    wall(l, {x, top - kSlab}, {half_width, kSlab});
    const Real pillar_half = (top - 2 * kSlab - kFloorTop) * 0.5f;
    if (pillar_half > 0) wall(l, {x, kFloorTop + pillar_half}, {0.2f, pillar_half});
}

int block(Layout& l, Material m, Vec2 pos, Vec2 half, Real angle) {
    Piece p;
    p.kind = Kind::Block;
    p.material = m;
    p.pos = pos;
    p.half = half;
    p.angle = angle;
    l.pieces.push_back(p);
    return static_cast<int>(l.pieces.size()) - 1;
}

int pig(Layout& l, Pig kind, Real x, Real base) {
    Piece p;
    p.kind = Kind::Pig;
    p.pig = kind;
    p.radius = pig_props(kind).radius;
    p.half = {p.radius, p.radius};
    p.pos = {x, base + p.radius + 0.001f};
    l.pieces.push_back(p);
    return static_cast<int>(l.pieces.size()) - 1;
}

int post(Layout& l, Material m, Real x, Real base, Real h, Real hw) {
    return block(l, m, {x, base + h * 0.5f + 0.001f}, {hw, h * 0.5f});
}

int plank(Layout& l, Material m, Real x, Real base, Real hw, Real t) {
    return block(l, m, {x, base + t * 0.5f + 0.001f}, {hw, t * 0.5f});
}

Real frame(Layout& l, Material m, Real x, Real base, Real span, Real h) {
    constexpr Real kPost = 0.12f, kPlank = 0.2f;
    post(l, m, x - span * 0.5f, base, h, kPost);
    post(l, m, x + span * 0.5f, base, h, kPost);
    const Real top = base + h + 0.001f;
    plank(l, m, x, top, span * 0.5f + kPost + 0.1f, kPlank);
    return top + kPlank + 0.001f;
}

int pyramid(Layout& l, Material m, Real x, Real base, int rows, Real half) {
    const Real pitch = 2 * half + 0.05f;
    const int first = static_cast<int>(l.pieces.size());
    for (int row = 0; row < rows; ++row)
        for (int i = 0; i < rows - row; ++i) {
            const Real px = x + (static_cast<Real>(i) - static_cast<Real>(rows - row - 1) * 0.5f) * pitch;
            const Real py = base + half + 0.001f + static_cast<Real>(row) * (2 * half + 0.001f);
            block(l, m, {px, py}, {half, half});
        }
    return first;
}

int column(Layout& l, Material m, Real x, Real base, int count, Vec2 half) {
    const int first = static_cast<int>(l.pieces.size());
    for (int i = 0; i < count; ++i) block(l, m, {x, base + half.y + 0.001f + static_cast<Real>(i) * (2 * half.y + 0.001f)}, half);
    return first;
}

}  // namespace kid
