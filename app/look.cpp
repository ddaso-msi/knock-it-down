#include "look.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace kid::app {

namespace {

constexpr float kPi = 3.14159265f;

SDL_FColor fc(SDL_Color c) { return {static_cast<float>(c.r) / 255.0f, static_cast<float>(c.g) / 255.0f, static_cast<float>(c.b) / 255.0f, static_cast<float>(c.a) / 255.0f}; }

SDL_Color mix(SDL_Color a, SDL_Color b, float t) {
    const auto m = [t](Uint8 x, Uint8 y) { return static_cast<Uint8>(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t); };
    return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a)};
}
SDL_Color with_alpha(SDL_Color c, Uint8 a) { return {c.r, c.g, c.b, a}; }
SDL_Color lighter(SDL_Color c, float t) { return mix(c, {255, 255, 255, c.a}, t); }
SDL_Color darker(SDL_Color c, float t) { return mix(c, {0, 0, 0, c.a}, t); }

// A small, repeatable random number from a seed and an index, in [0, 1).
float hashf(unsigned seed, unsigned i) {
    unsigned x = seed * 747796405u + i * 2891336453u + 1u;
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    return static_cast<float>(x & 0xffffff) / static_cast<float>(0x1000000);
}

// A quad with a colour at each corner (for gradients). Points in order round the quad.
void quad(SDL_Renderer* ren, SDL_FPoint p0, SDL_FPoint p1, SDL_FPoint p2, SDL_FPoint p3, SDL_Color c0, SDL_Color c1, SDL_Color c2, SDL_Color c3) {
    const SDL_Vertex v[4] = {{p0, fc(c0), {0, 0}}, {p1, fc(c1), {0, 0}}, {p2, fc(c2), {0, 0}}, {p3, fc(c3), {0, 0}}};
    const int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(ren, nullptr, v, 4, idx, 6);
}

void fill_ellipse(SDL_Renderer* ren, Vec2 centre, Vec2 along, Real a, Real b, SDL_Color c) {
    constexpr int kSides = 28;
    SDL_FPoint pts[kSides];
    const Vec2 across{-along.y, along.x};
    for (int k = 0; k < kSides; ++k) {
        const float t = 2 * kPi * static_cast<float>(k) / kSides;
        pts[k] = to_screen(centre + along * (a * std::cos(t)) + across * (b * std::sin(t)));
    }
    fill_polygon(ren, pts, kSides, c);
}

void outline(SDL_Renderer* ren, const SDL_FPoint* pts, int n, float width, SDL_Color c) {
    for (int i = 0; i < n; ++i) thick_line(ren, pts[i], pts[(i + 1) % n], width, c);
}

// A box body's four corners in world coordinates, counter-clockwise, and its half extents.
void corners(const Body& b, Vec2 out[4], Vec2& half) {
    const phys::Polygon& p = b.shape.polygon;
    half = {std::fabs(p.vertices[2].x), std::fabs(p.vertices[2].y)};
    for (int i = 0; i < 4; ++i) out[i] = apply(b.transform(), p.vertices[i]);
}

// A gentle silhouette of hills: the sum of a few sines, sampled across the view.
void hills(SDL_Renderer* ren, float base, float height, float depth, unsigned seed, SDL_Color top, SDL_Color bottom) {
    // Parallax: a far layer moves only `depth` as much as the world when the camera pans.
    const Vec2 centre = view_centre();
    const float shift = (centre.x - kWorldW * 0.5f) * (1 - depth);
    constexpr int kSteps = 64;
    const float x0 = -4, x1 = kWorldW + 4;
    for (int i = 0; i < kSteps; ++i) {
        const float xa = x0 + (x1 - x0) * static_cast<float>(i) / kSteps, xb = x0 + (x1 - x0) * static_cast<float>(i + 1) / kSteps;
        const auto h = [&](float x) {
            return base + height * (0.55f + 0.25f * std::sin(x * 0.31f + hashf(seed, 1) * 6) + 0.15f * std::sin(x * 0.83f + hashf(seed, 2) * 6) +
                                    0.05f * std::sin(x * 2.1f + hashf(seed, 3) * 6));
        };
        const SDL_FPoint ta = to_screen({xa + shift, h(xa)}), tb = to_screen({xb + shift, h(xb)});
        const SDL_FPoint ba = to_screen({xa + shift, -2}), bb = to_screen({xb + shift, -2});
        quad(ren, ta, tb, bb, ba, top, top, bottom, bottom);
    }
}

void castle(SDL_Renderer* ren, float x, float width, float height, SDL_Color c) {
    const Vec2 centre = view_centre();
    const float shift = (centre.x - kWorldW * 0.5f) * 0.7f;  // a far layer: depth 0.3
    const auto box = [&](float x0, float y0, float x1, float y1) {
        const SDL_FPoint a = to_screen({x0 + shift, y1}), b = to_screen({x1 + shift, y0});
        fill_rect(ren, {a.x, a.y, b.x - a.x, b.y - a.y}, c);
    };
    box(x, 0, x + width, height);  // the keep
    for (int k = 0; k < 4; ++k) {  // battlements
        const float bx = x + width * (static_cast<float>(k) + 0.1f) / 4.0f;
        box(bx, height, bx + width * 0.14f, height + 0.35f);
    }
    box(x - 0.6f, 0, x, height * 1.35f);  // towers either side
    box(x + width, 0, x + width + 0.6f, height * 1.2f);
}

}  // namespace

Theme theme_for(int level_index) {
    if (level_index >= 10)  // dusk at the castle
        return {{52, 36, 84, 255},   {236, 146, 104, 255}, {255, 196, 128, 255}, {120, 86, 120, 200}, {84, 56, 92, 255},
                {54, 52, 66, 255},   {86, 112, 62, 255},  {70, 54, 44, 255},    {104, 96, 104, 255}, {62, 56, 66, 255},
                false, true, 4.0f};
    if (level_index >= 5)  // snow
        return {{108, 140, 186, 255}, {218, 230, 244, 255}, {255, 250, 230, 255}, {244, 248, 255, 220}, {198, 212, 230, 255},
                {168, 188, 212, 255}, {236, 242, 250, 255}, {146, 156, 174, 255}, {136, 144, 160, 255}, {84, 92, 108, 255},
                true, false, 8.0f};
    return {{82, 146, 214, 255},  {196, 228, 246, 255}, {255, 242, 184, 255}, {255, 255, 255, 220}, {132, 182, 150, 255},
            {96, 156, 98, 255},   {112, 174, 72, 255},  {124, 92, 60, 255},   {138, 132, 124, 255}, {82, 78, 72, 255},
            false, false, 8.5f};
}

SDL_Color material_color(Material m) {
    switch (m) {
        case Material::Wood: return {176, 124, 72, 255};
        case Material::Stone: return {150, 150, 158, 255};
        case Material::Ice: return {178, 222, 244, 220};
    }
    return {176, 124, 72, 255};
}

void thick_line(SDL_Renderer* ren, SDL_FPoint a, SDL_FPoint b, float width, SDL_Color c) {
    const float dx = b.x - a.x, dy = b.y - a.y, len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-3f) return;
    const float nx = -dy / len * width * 0.5f, ny = dx / len * width * 0.5f;
    quad(ren, {a.x + nx, a.y + ny}, {b.x + nx, b.y + ny}, {b.x - nx, b.y - ny}, {a.x - nx, a.y - ny}, c, c, c, c);
}

void draw_backdrop(SDL_Renderer* ren, const Theme& t, double time) {
    // The sky: one tall gradient behind everything, from the top of the arena to the ground.
    const SDL_FPoint top_l = to_screen({-4, kWorldH + 4}), top_r = to_screen({kWorldW + 4, kWorldH + 4});
    const SDL_FPoint low_l = to_screen({-4, kFloorTop}), low_r = to_screen({kWorldW + 4, kFloorTop});
    fill_rect(ren, {0, 0, static_cast<float>(kWidth), static_cast<float>(kHeight)}, t.sky_low);
    quad(ren, top_l, top_r, low_r, low_l, t.sky_top, t.sky_top, t.sky_low, t.sky_low);

    // The sun with a soft glow, far away (it hardly moves with the camera).
    const Vec2 centre = view_centre();
    const Vec2 sun{15.5f + (centre.x - kWorldW * 0.5f) * 0.9f, t.sun_height};
    for (int r = 6; r >= 1; --r) fill_disc(ren, to_screen(sun), to_pixels(0.45f + 0.28f * static_cast<float>(r)), with_alpha(t.sun, static_cast<Uint8>(14 + 4 * (6 - r))));
    fill_disc(ren, to_screen(sun), to_pixels(0.55f), t.sun);

    // Clouds drifting slowly to the right, wrapping round.
    for (unsigned k = 0; k < 5; ++k) {
        const float speed = 0.15f + 0.1f * hashf(7, k), span = kWorldW + 8;
        const float x = std::fmod(hashf(3, k) * span + static_cast<float>(time) * speed, span) - 4 + (centre.x - kWorldW * 0.5f) * 0.8f;
        const float y = 6.5f + 3.0f * hashf(5, k), size = 0.6f + 0.5f * hashf(11, k);
        for (int puff = 0; puff < 4; ++puff) {
            const Vec2 p{x + (static_cast<float>(puff) - 1.5f) * size * 0.9f, y + (puff % 2 == 1 ? 0.25f : 0.0f) * size};
            fill_disc(ren, to_screen(p), to_pixels(size * (puff % 3 == 1 ? 0.75f : 0.55f)), t.cloud);
        }
    }

    if (t.castles) {
        castle(ren, 3.5f, 2.2f, 3.0f, t.far_hill);
        castle(ren, 12.5f, 3.0f, 3.8f, t.far_hill);
    }
    hills(ren, 0.5f, 3.2f, 0.3f, 17, t.far_hill, mix(t.far_hill, t.sky_low, 0.4f));
    hills(ren, 0.3f, 1.8f, 0.6f, 29, t.near_hill, darker(t.near_hill, 0.15f));
}

void draw_ground(SDL_Renderer* ren, const Theme& t) {
    const SDL_FPoint a = to_screen({-4, kFloorTop}), b = to_screen({kWorldW + 4, -3});
    fill_rect(ren, {a.x, a.y, b.x - a.x, b.y - a.y}, t.soil);
    // A band of grass (or snow) on top, a little uneven.
    constexpr int kSteps = 80;
    for (int i = 0; i < kSteps; ++i) {
        const float x0 = -4 + (kWorldW + 8) * static_cast<float>(i) / kSteps, x1 = -4 + (kWorldW + 8) * static_cast<float>(i + 1) / kSteps;
        const float h0 = 0.12f + 0.05f * std::sin(x0 * 3.1f), h1 = 0.12f + 0.05f * std::sin(x1 * 3.1f);
        quad(ren, to_screen({x0, kFloorTop + 0.02f}), to_screen({x1, kFloorTop + 0.02f}), to_screen({x1, kFloorTop - h1}),
             to_screen({x0, kFloorTop - h0}), lighter(t.grass, 0.15f), lighter(t.grass, 0.15f), t.grass, t.grass);
    }
}

void draw_shadow(SDL_Renderer* ren, const Body& b) {
    const Vec2 off{0.07f, -0.07f};
    const SDL_Color c{10, 14, 24, 60};
    if (b.shape.type == phys::Shape::Type::Circle) {
        fill_disc(ren, to_screen(b.pos + off), to_pixels(b.shape.circle.radius), c);
        return;
    }
    Vec2 v[4], half;
    corners(b, v, half);
    SDL_FPoint pts[4];
    for (int i = 0; i < 4; ++i) pts[i] = to_screen(v[i] + off);
    fill_polygon(ren, pts, 4, c);
}

void draw_block(SDL_Renderer* ren, const Body& b, Material m, float condition, unsigned seed) {
    if (b.shape.type == phys::Shape::Type::Circle) {  // a boulder or a wrecking ball
        const SDL_Color base = material_color(m);
        fill_disc(ren, to_screen(b.pos), to_pixels(b.shape.circle.radius), darker(base, 0.15f));
        fill_disc(ren, to_screen(b.pos + Vec2{-0.12f, 0.12f} * b.shape.circle.radius * 2.0f), to_pixels(b.shape.circle.radius * 0.6f),
                  lighter(base, 0.12f));
        return;
    }
    Vec2 v[4], half;
    corners(b, v, half);
    SDL_FPoint pts[4];
    for (int i = 0; i < 4; ++i) pts[i] = to_screen(v[i]);
    const Vec2 ux = b.q.x_axis(), uy{-ux.y, ux.x};
    const auto at = [&](float x, float y) { return to_screen(b.pos + ux * x + uy * y); };
    SDL_Color base = material_color(m);
    if (condition < 0.34f) base = darker(base, 0.25f);
    fill_polygon(ren, pts, 4, base);

    const bool long_x = half.x >= half.y;
    const float lx = long_x ? half.x : half.y, sy = long_x ? half.y : half.x;
    const auto along = [&](float s, float t) { return long_x ? at(s, t) : at(t, s); };
    if (m == Material::Wood) {
        // Grain: wavy lines along the long side, and a knot on bigger planks.
        const SDL_Color grain = darker(base, 0.22f);
        for (int line = 1; line <= 3; ++line) {
            const float t = sy * (-1 + 0.5f * static_cast<float>(line)) + sy * 0.12f * (hashf(seed, static_cast<unsigned>(line)) - 0.5f);
            SDL_FPoint prev = along(-lx * 0.92f, t);
            for (int k = 1; k <= 6; ++k) {
                const float s = -lx * 0.92f + lx * 1.84f * static_cast<float>(k) / 6.0f;
                const SDL_FPoint p = along(s, t + sy * 0.06f * std::sin(s * 9.0f + static_cast<float>(line)));
                thick_line(ren, prev, p, std::max(1.0f, to_pixels(0.018f)), grain);
                prev = p;
            }
        }
        if (lx > 0.45f) fill_disc(ren, along(lx * (hashf(seed, 9) - 0.5f), sy * 0.2f), to_pixels(sy * 0.22f), darker(base, 0.3f));
    } else if (m == Material::Stone) {
        // Speckle, and a bevelled inner edge.
        for (unsigned k = 0; k < 7; ++k)
            fill_disc(ren, at((hashf(seed, k) * 2 - 1) * half.x * 0.8f, (hashf(seed, k + 20) * 2 - 1) * half.y * 0.8f), to_pixels(0.025f + 0.02f * hashf(seed, k + 40)),
                      k % 2 ? darker(base, 0.2f) : lighter(base, 0.15f));
        const SDL_FPoint inner[4] = {at(-half.x * 0.82f, -half.y * 0.82f), at(half.x * 0.82f, -half.y * 0.82f), at(half.x * 0.82f, half.y * 0.82f),
                                     at(-half.x * 0.82f, half.y * 0.82f)};
        outline(ren, inner, 4, std::max(1.0f, to_pixels(0.02f)), lighter(base, 0.12f));
    } else {
        // Ice: two glints across it.
        const SDL_Color glint{255, 255, 255, 170};
        thick_line(ren, along(-lx * 0.6f, -sy * 0.4f), along(-lx * 0.1f, sy * 0.5f), std::max(1.5f, to_pixels(0.03f)), glint);
        thick_line(ren, along(lx * 0.1f, -sy * 0.5f), along(lx * 0.35f, 0), std::max(1.0f, to_pixels(0.02f)), glint);
    }

    // Light from above: the edge facing most upward is lit, the one facing down in shade.
    int top = 0, bottom = 0;
    float best_up = -2, best_down = 2;
    for (int i = 0; i < 4; ++i) {
        const Vec2 e = v[(i + 1) % 4] - v[i];
        const Vec2 n = Vec2{e.y, -e.x} * (1 / std::max(1e-6f, e.length()));  // outward, counter-clockwise polygon
        if (n.y > best_up) best_up = n.y, top = i;
        if (n.y < best_down) best_down = n.y, bottom = i;
    }
    const float w = std::max(1.5f, to_pixels(0.04f));
    outline(ren, pts, 4, std::max(1.5f, to_pixels(0.025f)), darker(base, 0.45f));
    thick_line(ren, pts[top], pts[(top + 1) % 4], w, lighter(base, m == Material::Ice ? 0.6f : 0.35f));
    thick_line(ren, pts[bottom], pts[(bottom + 1) % 4], w, darker(base, 0.35f));
}

void draw_rock(SDL_Renderer* ren, const Body& b, const Theme& t) {
    Vec2 v[4], half;
    corners(b, v, half);
    SDL_FPoint pts[4];
    for (int i = 0; i < 4; ++i) pts[i] = to_screen(v[i]);
    fill_polygon(ren, pts, 4, t.rock);
    // A few darker blotches, fixed to the rock.
    const auto at = [&](float x, float y) { return to_screen(b.pos + Vec2{x, y}); };
    for (unsigned k = 0; k < 6; ++k)
        fill_disc(ren, at((hashf(91, k) * 2 - 1) * half.x * 0.85f, (hashf(93, k) * 2 - 1) * half.y * 0.8f),
                  to_pixels(std::min(half.x, half.y) * (0.15f + 0.2f * hashf(95, k))), darker(t.rock, 0.08f));
    outline(ren, pts, 4, std::max(1.5f, to_pixels(0.03f)), t.rock_edge);
    // The top face catches the light (statics never turn, so the top is the top).
    thick_line(ren, at(-half.x, half.y), at(half.x, half.y), std::max(2.0f, to_pixels(0.06f)), lighter(t.rock, 0.3f));
}

void draw_pig(SDL_Renderer* ren, const Body& b, Pig kind, float condition, Vec2 look_at, double time, unsigned seed) {
    const Real r = b.shape.circle.radius;
    const SDL_Color skin = condition < 0.34f ? SDL_Color{126, 168, 92, 255} : SDL_Color{126, 200, 98, 255};
    const SDL_Color dark{70, 120, 56, 255};
    // Ears first, so the head covers their roots. They stay on top whichever way the pig rolls.
    for (const float side : {-1.0f, 1.0f}) {
        const Vec2 base = b.pos + Vec2{side * r * 0.55f, r * 0.7f};
        const SDL_FPoint ear[3] = {to_screen(base + Vec2{-side * r * 0.25f, 0}), to_screen(base + Vec2{side * r * 0.25f, -r * 0.05f}),
                                   to_screen(base + Vec2{side * r * 0.2f, r * 0.42f})};
        fill_polygon(ren, ear, 3, darker(skin, 0.1f));
    }
    fill_disc(ren, to_screen(b.pos), to_pixels(r), dark);
    fill_disc(ren, to_screen(b.pos), to_pixels(r * 0.93f), skin);
    fill_disc(ren, to_screen(b.pos + Vec2{-r * 0.3f, r * 0.35f}), to_pixels(r * 0.3f), lighter(skin, 0.18f));  // a sheen

    // The face looks the way the pig is looking: eyes shift towards `look_at`.
    Vec2 gaze = look_at - b.pos;
    gaze = gaze.length() > 1e-3f ? gaze * (1 / gaze.length()) : Vec2{-1, 0};
    const bool blink = std::fmod(time + hashf(seed, 1) * 5.0, 4.3) < 0.13;
    const bool hurt = condition < 0.6f;
    for (const float side : {-1.0f, 1.0f}) {
        const Vec2 eye = b.pos + Vec2{side * r * 0.36f, r * 0.2f};
        if (blink || hurt) {  // a closed eye, or a squeezed one: two short strokes
            const float tilt = hurt ? 0.12f * r : 0;
            thick_line(ren, to_screen(eye + Vec2{-r * 0.16f, tilt * side}), to_screen(eye + Vec2{r * 0.16f, -tilt * side}), std::max(2.0f, to_pixels(r * 0.08f)),
                       {30, 40, 30, 255});
            continue;
        }
        fill_disc(ren, to_screen(eye), to_pixels(r * 0.2f), {250, 252, 246, 255});
        fill_disc(ren, to_screen(eye + gaze * (r * 0.08f)), to_pixels(r * 0.095f), {24, 28, 24, 255});
    }
    // The snout, with nostrils.
    const Vec2 snout = b.pos + Vec2{gaze.x * r * 0.06f, -r * 0.2f};
    fill_ellipse(ren, snout, {1, 0}, r * 0.34f, r * 0.24f, lighter(skin, 0.12f));
    for (const float side : {-1.0f, 1.0f}) fill_disc(ren, to_screen(snout + Vec2{side * r * 0.12f, 0}), to_pixels(r * 0.06f), dark);

    if (kind == Pig::Helmet) {  // a steel cap over the top third
        SDL_FPoint cap[18];
        for (int i = 0; i < 17; ++i) {
            const float a = kPi * (0.1f + 0.8f * static_cast<float>(i) / 16.0f);
            cap[i] = to_screen(b.pos + Vec2{std::cos(a), std::sin(a) + 0.12f} * (r * 1.08f));
        }
        fill_polygon(ren, cap, 17, {150, 156, 170, 255});
        cap[17] = cap[0];
        for (int i = 0; i < 17; ++i) thick_line(ren, cap[i], cap[i + 1], std::max(1.5f, to_pixels(0.03f)), {206, 212, 224, 255});
    }
    if (kind == Pig::Big) {  // a moustache, for the boss
        for (const float side : {-1.0f, 1.0f})
            thick_line(ren, to_screen(snout + Vec2{0, -r * 0.24f}), to_screen(snout + Vec2{side * r * 0.5f, -r * 0.16f}), std::max(3.0f, to_pixels(r * 0.12f)),
                       {84, 60, 40, 255});
    }
}

void draw_bird(SDL_Renderer* ren, Vec2 pos, Real radius, Vec2 heading, Real stretch, Real squash, Ammo kind, bool egg) {
    if (egg) {  // an egg: pale, a little pointed upwards, speckled
        fill_ellipse(ren, pos, {0, 1}, radius * 1.15f, radius * 0.9f, {250, 244, 222, 255});
        fill_disc(ren, to_screen(pos + Vec2{radius * 0.3f, radius * 0.2f}), to_pixels(radius * 0.12f), {214, 200, 160, 255});
        return;
    }
    SDL_Color body;
    switch (kind) {
        case Ammo::Ball: body = {64, 152, 232, 255}; break;
        case Ammo::Heavy: body = {40, 84, 150, 255}; break;
        case Ammo::Blast: body = {222, 66, 84, 255}; break;
        case Ammo::Split: body = {56, 192, 196, 255}; break;
        case Ammo::Dart: body = {238, 196, 56, 255}; break;
        case Ammo::Dropper: body = {236, 236, 244, 255}; break;
    }
    const Vec2 along = heading, across{-heading.y, heading.x};
    const Real a = radius * stretch * (1 - 0.35f * squash), b = radius / stretch * (1 + 0.25f * squash);
    // Which way is up for the face: the side of the heading nearer the sky.
    const Vec2 up = across.y >= 0 ? across : across * -1.0f;
    const auto at = [&](float x, float y) { return pos + along * (x * a) + up * (y * b); };

    // Tail feathers behind, then the body.
    for (int k = -1; k <= 1; ++k)
        thick_line(ren, to_screen(at(-0.8f, 0.05f * static_cast<float>(k))), to_screen(at(-1.35f, 0.3f * static_cast<float>(k) + 0.1f)),
                   std::max(2.0f, to_pixels(radius * 0.16f)), darker(body, 0.3f));
    fill_ellipse(ren, pos, along, a, b, darker(body, 0.35f));
    fill_ellipse(ren, pos, along, a * 0.92f, b * 0.92f, body);
    fill_ellipse(ren, at(-0.05f, -0.35f), along, a * 0.5f, b * 0.38f, lighter(body, 0.35f));  // a pale belly
    // A wing folded along the side, so the shape says "bird" before the face does.
    {
        const SDL_FPoint wing[4] = {to_screen(at(-0.55f, 0.05f)), to_screen(at(0.05f, 0.18f)), to_screen(at(-0.05f, -0.22f)),
                                    to_screen(at(-0.75f, -0.32f))};
        fill_polygon(ren, wing, 4, darker(body, 0.22f));
    }

    if (kind == Ammo::Blast) {  // a fuse with a spark
        thick_line(ren, to_screen(at(0, 0.9f)), to_screen(at(0.15f, 1.35f)), std::max(2.0f, to_pixels(0.04f)), {60, 50, 40, 255});
        fill_disc(ren, to_screen(at(0.15f, 1.4f)), to_pixels(0.07f), {255, 210, 90, 255});
    }
    if (kind == Ammo::Dart) {  // a swept crest
        const SDL_FPoint crest[3] = {to_screen(at(-0.1f, 0.8f)), to_screen(at(0.4f, 0.85f)), to_screen(at(-0.6f, 1.4f))};
        fill_polygon(ren, crest, 3, darker(body, 0.15f));
    }
    if (kind == Ammo::Dropper) {  // a tuft
        for (int k = 0; k < 3; ++k)
            thick_line(ren, to_screen(at(0.0f, 0.85f)), to_screen(at(-0.25f + 0.25f * static_cast<float>(k), 1.3f)), std::max(1.5f, to_pixels(0.03f)), {180, 180, 196, 255});
    }

    // The eye, looking ahead, under a stern brow (bushier on the heavy bird); then the beak.
    const Vec2 eye = at(0.36f, 0.3f);
    fill_disc(ren, to_screen(eye), std::max(2.5f, to_pixels(radius * 0.32f)), {252, 252, 248, 255});
    fill_disc(ren, to_screen(eye + along * (radius * 0.11f)), std::max(1.5f, to_pixels(radius * 0.15f)), {20, 24, 30, 255});
    const float brow = kind == Ammo::Heavy ? radius * 0.2f : radius * 0.11f;
    thick_line(ren, to_screen(at(0.12f, 0.62f)), to_screen(at(0.66f, 0.46f)), std::max(2.0f, to_pixels(brow)), {40, 30, 30, 255});
    const SDL_FPoint beak[3] = {to_screen(at(0.74f, 0.16f)), to_screen(at(1.45f, -0.06f)), to_screen(at(0.74f, -0.32f))};
    fill_polygon(ren, beak, 3, {250, 184, 60, 255});
}

void draw_sling(SDL_Renderer* ren, Vec2 pouch, std::optional<Vec2> bird, Real bird_radius, Vec2 wobble) {
    const SDL_Color wood{120, 80, 46, 255}, wood_light{160, 112, 66, 255}, rubber{70, 40, 34, 255};
    const Vec2 fork = pouch + Vec2{0, -0.5f}, back = pouch + Vec2{-0.24f, 0.16f}, front = pouch + Vec2{0.24f, 0.16f};
    const float w = std::max(4.0f, to_pixels(0.11f)), band = std::max(3.0f, to_pixels(0.06f));
    // The back band and the back prong go behind the bird; the front ones in front of it.
    const Vec2 meet = bird ? *bird : pouch + Vec2{0, 0.1f} + wobble;
    thick_line(ren, to_screen(fork), to_screen(back), w, darker(wood, 0.15f));
    thick_line(ren, to_screen(back), to_screen(meet), band, rubber);
    if (bird) fill_ellipse(ren, *bird + Vec2{-bird_radius * 0.75f, 0}, {1, 0}, bird_radius * 0.3f, bird_radius * 0.75f, {92, 60, 40, 255});  // the pouch
    thick_line(ren, to_screen({pouch.x, kFloorTop}), to_screen(fork), w * 1.2f, wood);
    thick_line(ren, to_screen({pouch.x - 0.03f, kFloorTop}), to_screen(fork + Vec2{-0.03f, 0}), w * 0.3f, wood_light);
    thick_line(ren, to_screen(fork), to_screen(front), w, wood);
}

void draw_sling_front(SDL_Renderer* ren, Vec2 pouch, std::optional<Vec2> bird, Vec2 wobble) {
    const Vec2 front = pouch + Vec2{0.24f, 0.16f};
    // The front band holds the pouch behind the bird: it ends at the bird's back, not across its face.
    const Vec2 meet = bird ? *bird + Vec2{-0.2f, 0} : pouch + Vec2{0, 0.1f} + wobble;
    thick_line(ren, to_screen(front), to_screen(meet), std::max(3.0f, to_pixels(0.06f)), {90, 52, 42, 255});
}

void Weather::update(const Theme& t, float dt) {
    time_ += dt;
    if (!t.snow) {
        flakes_.clear();
        return;
    }
    // Screen-space flakes: they fall past whatever the camera is doing.
    while (flakes_.size() < 90) {
        const unsigned k = static_cast<unsigned>(flakes_.size()) + static_cast<unsigned>(time_ * 1000);
        const bool first = time_ < 0.5f;  // the first fill covers the whole view, later ones start above it
        flakes_.push_back({hashf(41, k) * static_cast<float>(kWidth), (first ? 1 : -1) * hashf(43, k) * static_cast<float>(kHeight), 25 + 35 * hashf(47, k),
                           1.5f + 2.0f * hashf(53, k), hashf(59, k) * 6.28f});
    }
    for (Flake& f : flakes_) {
        f.y += f.speed * dt;
        f.x += std::sin(time_ * 0.8f + f.sway) * 12.0f * dt;
        if (f.y > static_cast<float>(kHeight)) {
            f.y -= static_cast<float>(kHeight) + 10;
            f.x = std::fmod(f.x + 377.0f, static_cast<float>(kWidth));
        }
    }
}

void Weather::draw(SDL_Renderer* ren) const {
    for (const Flake& f : flakes_) fill_disc(ren, {f.x, f.y}, f.size, {255, 255, 255, 200});
}

}  // namespace kid::app
