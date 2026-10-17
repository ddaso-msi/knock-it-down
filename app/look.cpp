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
SDL_Color with_alpha(SDL_Color c, float a) { return {c.r, c.g, c.b, static_cast<Uint8>(std::clamp(a, 0.0f, 255.0f))}; }

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

// A box body's four corners in world coordinates, counter-clockwise, and its half extents.
void corners(const Body& b, Vec2 out[4], Vec2& half) {
    const phys::Polygon& p = b.shape.polygon;
    half = {std::fabs(p.vertices[2].x), std::fabs(p.vertices[2].y)};
    for (int i = 0; i < 4; ++i) out[i] = apply(b.transform(), p.vertices[i]);
}

// Rim light: each edge of a box glows in proportion to how squarely it faces the light.
void rim_box(SDL_Renderer* ren, const Vec2 v[4], Vec2 light, SDL_Color c, float strength = 1) {
    const float w = std::max(1.5f, to_pixels(0.035f));
    for (int i = 0; i < 4; ++i) {
        const Vec2 a = v[i], b = v[(i + 1) % 4], e = b - a;
        const float len = e.length();
        if (len < 1e-4f) continue;
        const Vec2 n{e.y / len, -e.x / len};  // outward: the polygon is counter-clockwise
        Vec2 to = light - (a + b) * 0.5f;
        to = to * (1 / std::max(0.01f, to.length()));
        const float lit = dot(n, to);
        if (lit > 0.15f) thick_line(ren, to_screen(a), to_screen(b), w, with_alpha(c, (60 + 190 * lit) * strength));
    }
}

// The same for a disc: an arc of light on the side towards the sun.
void rim_disc(SDL_Renderer* ren, Vec2 centre, Real r, Vec2 light, SDL_Color c) {
    Vec2 to = light - centre;
    to = to * (1 / std::max(0.01f, to.length()));
    const float facing = std::atan2(to.y, to.x), w = std::max(1.5f, to_pixels(0.035f));
    for (int k = -5; k < 5; ++k) {
        const float a0 = facing + static_cast<float>(k) * 0.13f, a1 = a0 + 0.13f;
        const float strength = 1 - std::fabs(static_cast<float>(k) + 0.5f) / 5.5f;
        thick_line(ren, to_screen(centre + Vec2{std::cos(a0), std::sin(a0)} * r), to_screen(centre + Vec2{std::cos(a1), std::sin(a1)} * r), w,
                   with_alpha(c, 230 * strength));
    }
}

void hills(SDL_Renderer* ren, float base, float height, float depth, unsigned seed, SDL_Color fill) {
    // Parallax: a far layer moves only `depth` as much as the world when the camera pans.
    const Vec2 centre = view_centre();
    const float shift = (centre.x - kWorldW * 0.5f) * (1 - depth);
    constexpr int kSteps = 72;
    const float x0 = -4, x1 = kWorldW + 4;
    for (int i = 0; i < kSteps; ++i) {
        const float xa = x0 + (x1 - x0) * static_cast<float>(i) / kSteps, xb = x0 + (x1 - x0) * static_cast<float>(i + 1) / kSteps;
        const auto h = [&](float x) {
            return base + height * (0.5f + 0.3f * std::sin(x * 0.4f + hashf(seed, 1) * 6) + 0.15f * std::sin(x * 1.3f + hashf(seed, 2) * 6) +
                                    0.05f * std::sin(x * 3.1f + hashf(seed, 3) * 6));
        };
        quad(ren, to_screen({xa + shift, h(xa)}), to_screen({xb + shift, h(xb)}), to_screen({xb + shift, -2}), to_screen({xa + shift, -2}), fill, fill, fill,
             fill);
    }
}

// A band of haze lying over everything below `top`, thickest at `bottom`.
void haze(SDL_Renderer* ren, float top, float bottom, SDL_Color c, float alpha) {
    const float y0 = to_screen({0, top}).y, y1 = to_screen({0, bottom}).y, w = static_cast<float>(kWidth);
    quad(ren, {0, y0}, {w, y0}, {w, y1}, {0, y1}, with_alpha(c, 0), with_alpha(c, 0), with_alpha(c, alpha), with_alpha(c, alpha));
}

void castle(SDL_Renderer* ren, float x, float width, float height, float depth, SDL_Color c) {
    const float shift = (view_centre().x - kWorldW * 0.5f) * (1 - depth);
    const auto box = [&](float x0, float y0, float x1, float y1) {
        const SDL_FPoint a = to_screen({x0 + shift, y1}), b = to_screen({x1 + shift, y0});
        fill_rect(ren, {a.x, a.y, b.x - a.x, b.y - a.y}, c);
    };
    box(x, 0, x + width, height);  // the keep, battlements, towers either side
    for (int k = 0; k < 4; ++k) {
        const float bx = x + width * (static_cast<float>(k) + 0.1f) / 4.0f;
        box(bx, height, bx + width * 0.14f, height + 0.35f);
    }
    box(x - 0.6f, 0, x, height * 1.35f);
    box(x + width, 0, x + width + 0.6f, height * 1.2f);
    box(x - 0.45f, height * 1.35f, x - 0.15f, height * 1.35f + 0.5f);
}

}  // namespace

Theme theme_for(int level_index) {
    Theme t;
    if (level_index >= 10) {  // embers at the castle: a red sky, a huge low sun, sparks rising
        t = {{16, 8, 12, 255}, {74, 24, 26, 255}, {214, 86, 44, 255}, {255, 160, 90, 255}, {200, 80, 44, 255}, {255, 214, 120, 255}, {13.8f, 2.4f}, 1.3f};
        t.castles = t.embers = true;
    } else if (level_index >= 5) {  // a cold night under a full moon, snowing
        t = {{6, 10, 22, 255}, {28, 46, 78, 255}, {104, 146, 184, 255}, {226, 238, 255, 255}, {120, 160, 196, 255}, {150, 255, 170, 255}, {15.2f, 6.0f}, 0.75f};
        t.snow = true;
    } else {  // amber dusk, the sun just behind the target
        t = {{22, 18, 34, 255}, {96, 52, 64, 255}, {232, 140, 84, 255}, {255, 214, 150, 255}, {232, 140, 84, 255}, {170, 255, 120, 255}, {14.6f, 3.2f}, 1.0f};
    }
    return t;
}

SDL_Color material_color(Material m) {
    switch (m) {
        case Material::Wood: return {46, 30, 22, 255};
        case Material::Stone: return {34, 36, 44, 255};
        case Material::Ice: return {120, 190, 220, 220};
    }
    return {46, 30, 22, 255};
}

SDL_Color bird_color(Ammo kind) {
    switch (kind) {
        case Ammo::Ball: return {90, 170, 255, 255};
        case Ammo::Heavy: return {150, 150, 255, 255};
        case Ammo::Blast: return {255, 90, 70, 255};
        case Ammo::Split: return {80, 230, 220, 255};
        case Ammo::Dart: return {255, 210, 80, 255};
        case Ammo::Dropper: return {240, 240, 255, 255};
    }
    return {90, 170, 255, 255};
}

void thick_line(SDL_Renderer* ren, SDL_FPoint a, SDL_FPoint b, float width, SDL_Color c) {
    const float dx = b.x - a.x, dy = b.y - a.y, len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-3f) return;
    const float nx = -dy / len * width * 0.5f, ny = dx / len * width * 0.5f;
    quad(ren, {a.x + nx, a.y + ny}, {b.x + nx, b.y + ny}, {b.x - nx, b.y - ny}, {a.x - nx, a.y - ny}, c, c, c, c);
}

void glow(SDL_Renderer* ren, SDL_FPoint centre, float radius, SDL_Color inner) {
    constexpr int kSides = 40;
    SDL_Vertex v[kSides + 1];
    int idx[kSides * 3];
    v[0] = {centre, fc(inner), {0, 0}};
    for (int i = 0; i < kSides; ++i) {
        const float a = 2 * kPi * static_cast<float>(i) / kSides;
        v[i + 1] = {{centre.x + radius * std::cos(a), centre.y + radius * std::sin(a)}, fc(with_alpha(inner, 0)), {0, 0}};
        idx[i * 3] = 0;
        idx[i * 3 + 1] = i + 1;
        idx[i * 3 + 2] = (i + 1) % kSides + 1;
    }
    SDL_RenderGeometry(ren, nullptr, v, kSides + 1, idx, kSides * 3);
}

void draw_backdrop(SDL_Renderer* ren, const Theme& t, double time) {
    // The sky in three bands, fixed to the window (it is far away), from deep overhead to the glow
    // at the horizon.
    const float w = static_cast<float>(kWidth), h = static_cast<float>(kHeight);
    const float horizon = to_screen({0, kFloorTop}).y;
    quad(ren, {0, 0}, {w, 0}, {w, horizon * 0.45f}, {0, horizon * 0.45f}, t.sky_top, t.sky_top, t.sky_mid, t.sky_mid);
    quad(ren, {0, horizon * 0.45f}, {w, horizon * 0.45f}, {w, h}, {0, h}, t.sky_mid, t.sky_mid, t.sky_low, t.sky_low);
    // Stars, faint, twinkling, only where the sky is dark enough.
    for (unsigned k = 0; k < 90; ++k) {
        const float x = hashf(71, k) * w, y = hashf(73, k) * horizon * 0.55f;
        const float twinkle = 0.6f + 0.4f * std::sin(static_cast<float>(time) * (0.8f + hashf(79, k)) + hashf(83, k) * 6);
        fill_disc(ren, {x, y}, 0.6f + 1.0f * hashf(89, k), with_alpha({255, 240, 225, 255}, 150 * twinkle * (1 - y / (horizon * 0.55f))));
    }
    // The sun (or moon), far off: it barely moves with the camera. A wide bloom, then the disc.
    const Vec2 sun = t.sun_at + Vec2{(view_centre().x - kWorldW * 0.5f) * 0.9f, 0};
    const SDL_FPoint s = to_screen(sun);
    glow(ren, s, to_pixels(t.sun_size * 9), with_alpha(t.sun, 90));
    glow(ren, s, to_pixels(t.sun_size * 3), with_alpha(t.sun, 200));
    fill_disc(ren, s, to_pixels(t.sun_size), t.sun);

    // Hills in layers, each darker and less hazy than the one behind, with haze lying between them.
    if (t.castles) {
        castle(ren, 3.0f, 2.2f, 3.4f, 0.25f, mix(t.sky_low, kInk, 0.55f));
        castle(ren, 15.5f, 2.8f, 4.2f, 0.25f, mix(t.sky_low, kInk, 0.55f));
    }
    hills(ren, 2.4f, 3.0f, 0.2f, 3, mix(t.sky_low, kInk, 0.45f));
    haze(ren, 4.2f, 1.0f, t.haze, 120);
    hills(ren, 1.2f, 2.2f, 0.45f, 7, mix(t.sky_low, kInk, 0.68f));
    haze(ren, 2.6f, 0.6f, t.haze, 90);
    hills(ren, 0.4f, 1.3f, 0.75f, 11, mix(t.sky_low, kInk, 0.86f));
    haze(ren, 1.4f, 0.3f, t.haze, 40);
}

void draw_ground(SDL_Renderer* ren, const Theme& t) {
    const SDL_FPoint a = to_screen({-4, kFloorTop}), b = to_screen({kWorldW + 4, -3});
    fill_rect(ren, {a.x, a.y, b.x - a.x, b.y - a.y}, kInk);
    // Blades of grass along the edge (on snowy nights, a pale rim of snow instead).
    for (unsigned i = 0; i < 300; ++i) {
        const float x = -4 + (kWorldW + 8) * static_cast<float>(i) / 300 + 0.03f * hashf(5, i);
        const float h = t.snow ? 0.03f + 0.03f * hashf(7, i) : 0.06f + 0.16f * hashf(7, i);
        thick_line(ren, to_screen({x, kFloorTop - 0.02f}), to_screen({x + 0.06f * (hashf(6, i) - 0.5f), kFloorTop + h}), std::max(1.5f, to_pixels(0.03f)), kInk);
    }
    if (t.snow) thick_line(ren, to_screen({-4, kFloorTop}), to_screen({kWorldW + 4, kFloorTop}), std::max(2.0f, to_pixels(0.04f)), {200, 220, 240, 160});
}

void gradient_rect(SDL_Renderer* ren, SDL_FRect r, SDL_Color top, SDL_Color bottom) {
    quad(ren, {r.x, r.y}, {r.x + r.w, r.y}, {r.x + r.w, r.y + r.h}, {r.x, r.y + r.h}, top, top, bottom, bottom);
}

void draw_vignette(SDL_Renderer* ren) {
    // Four soft gradients from the edges inwards: black at the edge, gone a fifth of the way in.
    const float w = static_cast<float>(kWidth), h = static_cast<float>(kHeight), mx = w * 0.18f, my = h * 0.22f;
    const SDL_Color edge{0, 0, 0, 150}, none{0, 0, 0, 0};
    quad(ren, {0, 0}, {mx, 0}, {mx, h}, {0, h}, edge, none, none, edge);
    quad(ren, {w - mx, 0}, {w, 0}, {w, h}, {w - mx, h}, none, edge, edge, none);
    quad(ren, {0, 0}, {w, 0}, {w, my}, {0, my}, edge, edge, none, none);
    quad(ren, {0, h - my}, {w, h - my}, {w, h}, {0, h}, none, none, edge, edge);
}

void draw_block(SDL_Renderer* ren, const Body& b, Material m, float condition, unsigned seed, const Theme& t) {
    const SDL_Color base = material_color(m);
    if (b.shape.type == phys::Shape::Type::Circle) {  // a boulder or a wrecking ball
        fill_disc(ren, to_screen(b.pos), to_pixels(b.shape.circle.radius), m == Material::Ice ? base : kInk);
        rim_disc(ren, b.pos, b.shape.circle.radius, t.sun_at, t.sun);
        return;
    }
    Vec2 v[4], half;
    corners(b, v, half);
    SDL_FPoint pts[4];
    for (int i = 0; i < 4; ++i) pts[i] = to_screen(v[i]);
    const Vec2 ux = b.q.x_axis(), uy{-ux.y, ux.x};
    const auto at = [&](float x, float y) { return to_screen(b.pos + ux * x + uy * y); };
    const bool long_x = half.x >= half.y;
    const float lx = long_x ? half.x : half.y, sy = long_x ? half.y : half.x;
    const auto along = [&](float s, float u) { return long_x ? at(s, u) : at(u, s); };

    if (m == Material::Ice) {
        // Ice: the one thing that lets light through. A dark glassy tint, cold glints, a bright rim.
        fill_polygon(ren, pts, 4, {30, 54, 70, 200});
        const SDL_Color glint{190, 235, 255, 150};
        thick_line(ren, along(-lx * 0.6f, -sy * 0.4f), along(-lx * 0.1f, sy * 0.5f), std::max(1.5f, to_pixels(0.03f)), glint);
        thick_line(ren, along(lx * 0.1f, -sy * 0.5f), along(lx * 0.35f, 0), std::max(1.0f, to_pixels(0.02f)), glint);
        rim_box(ren, v, t.sun_at, {190, 235, 255, 255});
    } else {
        fill_polygon(ren, pts, 4, base);
        if (m == Material::Wood) {  // grain, barely there: you see it when the light catches it
            for (int line = 1; line <= 2; ++line) {
                const float u = sy * (-1 + 0.66f * static_cast<float>(line));
                thick_line(ren, along(-lx * 0.9f, u), along(lx * 0.9f, u + sy * 0.08f * (hashf(seed, static_cast<unsigned>(line)) - 0.5f)),
                           std::max(1.0f, to_pixels(0.015f)), with_alpha(t.sun, 26));
            }
        } else {  // stone: a few chips catching the light
            for (unsigned k = 0; k < 5; ++k)
                fill_disc(ren, at((hashf(seed, k) * 2 - 1) * half.x * 0.8f, (hashf(seed, k + 20) * 2 - 1) * half.y * 0.8f), std::max(1.0f, to_pixels(0.022f)),
                          with_alpha(t.sun, 40));
        }
        rim_box(ren, v, t.sun_at, t.sun, m == Material::Stone ? 0.8f : 1.0f);
    }

    // Damage: cracks that glow like embers, brighter the closer it is to breaking.
    if (condition < 0.66f) {
        const float heat = condition < 0.33f ? 1.0f : 0.55f;
        const SDL_Color ember{255, 150, 70, static_cast<Uint8>(200 * heat)};
        const float r = std::min(half.x, half.y) * 1.6f;
        const auto c = [&](float x, float y) { return to_screen(b.pos + rotate(b.q, Vec2{x, y} * r)); };
        const float lw = std::max(1.5f, to_pixels(0.025f));
        thick_line(ren, c(-0.6f, 0.5f), c(-0.1f, 0.05f), lw, ember);
        thick_line(ren, c(-0.1f, 0.05f), c(0.15f, 0.3f), lw, ember);
        thick_line(ren, c(0.15f, 0.3f), c(0.6f, -0.2f), lw, ember);
        if (condition < 0.33f) {
            thick_line(ren, c(-0.4f, -0.6f), c(0.0f, -0.15f), lw, ember);
            thick_line(ren, c(0.0f, -0.15f), c(0.45f, -0.5f), lw, ember);
        }
    }
}

void draw_rock(SDL_Renderer* ren, const Body& b, const Theme& t) {
    Vec2 v[4], half;
    corners(b, v, half);
    SDL_FPoint pts[4];
    for (int i = 0; i < 4; ++i) pts[i] = to_screen(v[i]);
    fill_polygon(ren, pts, 4, kInk);
    rim_box(ren, v, t.sun_at, t.sun, 0.7f);
}

void draw_pig(SDL_Renderer* ren, const Body& b, Pig kind, float condition, Vec2 look_at, double time, unsigned seed, const Theme& t) {
    const Real r = b.shape.circle.radius;
    // Two small ears, then the round body, all ink.
    for (const float side : {-1.0f, 1.0f}) {
        const Vec2 base = b.pos + Vec2{side * r * 0.55f, r * 0.7f};
        const SDL_FPoint ear[3] = {to_screen(base + Vec2{-side * r * 0.22f, 0}), to_screen(base + Vec2{side * r * 0.22f, -r * 0.05f}),
                                   to_screen(base + Vec2{side * r * 0.16f, r * 0.36f})};
        fill_polygon(ren, ear, 3, kInk);
    }
    fill_disc(ren, to_screen(b.pos), to_pixels(r), kInk);
    if (kind == Pig::Helmet) {  // a steel cap: the one hard edge on a pig, catching the light
        SDL_FPoint cap[17];
        for (int i = 0; i < 17; ++i) {
            const float a = kPi * (0.1f + 0.8f * static_cast<float>(i) / 16.0f);
            cap[i] = to_screen(b.pos + Vec2{std::cos(a), std::sin(a) + 0.12f} * (r * 1.08f));
        }
        fill_polygon(ren, cap, 17, {28, 30, 38, 255});
        for (int i = 0; i < 16; ++i) thick_line(ren, cap[i], cap[i + 1], std::max(1.5f, to_pixels(0.03f)), with_alpha(t.sun, 140));
    }
    rim_disc(ren, b.pos, r, t.sun_at, t.sun);

    // The eyes: two points of light, looking at the bird. Hurt, they burn orange and one narrows.
    Vec2 gaze = look_at - b.pos;
    gaze = gaze.length() > 1e-3f ? gaze * (1 / gaze.length()) : Vec2{-1, 0};
    const bool blink = std::fmod(time + hashf(seed, 1) * 5.0, 4.3) < 0.12;
    if (blink) return;
    const bool hurt = condition < 0.6f;
    const SDL_Color eye = hurt ? SDL_Color{255, 150, 60, 255} : t.eyes;
    const float size = kind == Pig::Big ? 0.11f : 0.09f;
    for (const float side : {-1.0f, 1.0f}) {
        const Vec2 e = b.pos + Vec2{side * r * 0.32f, r * 0.15f} + gaze * (r * 0.08f);
        const float s = hurt && side > 0 ? 0.6f : 1.0f;
        glow(ren, to_screen(e), to_pixels(r * 0.4f), with_alpha(eye, 120));
        fill_ellipse(ren, e, {1, 0}, r * size * 1.3f, r * size * s, mix(eye, {255, 255, 255, 255}, 0.4f));
    }
}

void draw_bird(SDL_Renderer* ren, Vec2 pos, Real radius, Vec2 heading, Real stretch, Real squash, Ammo kind, bool egg, const Theme& t) {
    if (egg) {
        fill_ellipse(ren, pos, {0, 1}, radius * 1.15f, radius * 0.9f, kInk);
        rim_disc(ren, pos, radius, t.sun_at, t.sun);
        glow(ren, to_screen(pos), to_pixels(radius * 1.6f), with_alpha({255, 140, 80, 255}, 90));  // it is about to go off
        return;
    }
    const Vec2 along = heading, across{-heading.y, heading.x};
    const Real a = radius * stretch * (1 - 0.35f * squash), b = radius / stretch * (1 + 0.25f * squash);
    const Vec2 up = across.y >= 0 ? across : across * -1.0f;  // the side of the heading nearer the sky
    const auto at = [&](float x, float y) { return pos + along * (x * a) + up * (y * b); };
    const SDL_Color streak = bird_color(kind);

    // A swept tail, the body, a beak: all ink.
    const SDL_FPoint tail[3] = {to_screen(at(-0.7f, 0.35f)), to_screen(at(-1.6f, 0.15f)), to_screen(at(-0.7f, -0.3f))};
    fill_polygon(ren, tail, 3, kInk);
    fill_ellipse(ren, pos, along, a, b, kInk);
    const SDL_FPoint beak[3] = {to_screen(at(0.75f, 0.2f)), to_screen(at(1.45f, -0.05f)), to_screen(at(0.75f, -0.3f))};
    fill_polygon(ren, beak, 3, kInk);
    if (kind == Ammo::Dart) {  // a swept crest
        const SDL_FPoint crest[3] = {to_screen(at(-0.1f, 0.8f)), to_screen(at(0.4f, 0.85f)), to_screen(at(-0.7f, 1.45f))};
        fill_polygon(ren, crest, 3, kInk);
    }
    if (kind == Ammo::Heavy) {  // a heavy brow ridge
        thick_line(ren, to_screen(at(0.05f, 0.7f)), to_screen(at(0.7f, 0.5f)), std::max(3.0f, to_pixels(radius * 0.22f)), kInk);
    }
    rim_disc(ren, pos, std::max(a, b), t.sun_at, t.sun);

    // Its kind's colour, as a streak along the back: the one bright thing about it.
    thick_line(ren, to_screen(at(-0.75f, 0.45f)), to_screen(at(0.35f, 0.62f)), std::max(2.0f, to_pixels(radius * 0.13f)), with_alpha(streak, 220));
    if (kind == Ammo::Blast) {  // a fuse, burning
        thick_line(ren, to_screen(at(0, 0.9f)), to_screen(at(0.15f, 1.35f)), std::max(2.0f, to_pixels(0.04f)), kInk);
        glow(ren, to_screen(at(0.15f, 1.4f)), to_pixels(0.25f), {255, 170, 80, 220});
        fill_disc(ren, to_screen(at(0.15f, 1.4f)), std::max(2.0f, to_pixels(0.04f)), {255, 236, 190, 255});
    }
    // One eye, bright.
    const SDL_FPoint eye = to_screen(at(0.42f, 0.25f));
    glow(ren, eye, to_pixels(radius * 0.45f), {255, 255, 255, 110});
    fill_disc(ren, eye, std::max(1.8f, to_pixels(radius * 0.11f)), {255, 255, 250, 255});
}

void draw_sling(SDL_Renderer* ren, Vec2 pouch, std::optional<Vec2> bird, Real bird_radius, Vec2 wobble, const Theme& t) {
    const SDL_Color rubber{30, 20, 18, 255};
    const Vec2 fork = pouch + Vec2{0, -0.5f}, back = pouch + Vec2{-0.24f, 0.16f}, front = pouch + Vec2{0.24f, 0.16f};
    const float w = std::max(4.0f, to_pixels(0.11f)), band = std::max(2.5f, to_pixels(0.05f));
    const Vec2 meet = bird ? *bird : pouch + Vec2{0, 0.1f} + wobble;
    thick_line(ren, to_screen(fork), to_screen(back), w, kInk);
    thick_line(ren, to_screen(back), to_screen(meet), band, rubber);
    if (bird) fill_ellipse(ren, *bird + Vec2{-bird_radius * 0.75f, 0}, {1, 0}, bird_radius * 0.3f, bird_radius * 0.75f, rubber);  // the pouch
    thick_line(ren, to_screen({pouch.x, kFloorTop}), to_screen(fork), w * 1.2f, kInk);
    thick_line(ren, to_screen(fork), to_screen(front), w, kInk);
    // A thin line of light down the sunward side of the post.
    thick_line(ren, to_screen({pouch.x + 0.05f, kFloorTop}), to_screen(fork + Vec2{0.05f, 0}), std::max(1.0f, to_pixels(0.02f)), with_alpha(t.sun, 120));
}

void draw_sling_front(SDL_Renderer* ren, Vec2 pouch, std::optional<Vec2> bird, Vec2 wobble) {
    const Vec2 front = pouch + Vec2{0.24f, 0.16f};
    // The front band holds the pouch behind the bird: it ends at the bird's back, not across its face.
    const Vec2 meet = bird ? *bird + Vec2{-0.2f, 0} : pouch + Vec2{0, 0.1f} + wobble;
    thick_line(ren, to_screen(front), to_screen(meet), std::max(2.5f, to_pixels(0.05f)), {30, 20, 18, 255});
}

void Weather::update(const Theme& t, float dt) {
    time_ += dt;
    const int kind = t.snow ? 1 : t.embers ? 2 : 0;
    if (kind != kind_) {
        motes_.clear();
        kind_ = kind;
    }
    const size_t want = kind == 1 ? 110 : kind == 2 ? 70 : 60;
    while (motes_.size() < want) {  // screen-space: they drift past whatever the camera is doing
        const unsigned k = static_cast<unsigned>(motes_.size()) + static_cast<unsigned>(time_ * 1000);
        const bool first = time_ < 0.5f;  // the first fill covers the whole view, later ones start off it
        const float y = first ? hashf(43, k) * static_cast<float>(kHeight) : kind == 2 ? static_cast<float>(kHeight) + 10 : -10;
        motes_.push_back({hashf(41, k) * static_cast<float>(kWidth), y, kind == 1 ? 25 + 35 * hashf(47, k) : 8 + 18 * hashf(47, k),
                          kind == 1 ? 1.5f + 2.0f * hashf(53, k) : 0.8f + 1.4f * hashf(53, k), hashf(59, k) * 6.28f});
    }
    for (Mote& m : motes_) {
        m.y += (kind == 2 ? -m.speed : kind == 1 ? m.speed : m.speed * 0.15f) * dt;  // sparks rise, snow falls, dust hangs
        m.x += std::sin(time_ * 0.7f + m.sway) * (kind == 0 ? 6.0f : 12.0f) * dt;
        if (m.y > static_cast<float>(kHeight) + 12 || m.y < -12) {
            m.y = kind == 2 ? static_cast<float>(kHeight) + 10 : -10;
            m.x = std::fmod(m.x + 377.0f, static_cast<float>(kWidth));
        }
    }
}

void Weather::draw(SDL_Renderer* ren, const Theme& t) const {
    for (const Mote& m : motes_) {
        if (kind_ == 1) {
            fill_disc(ren, {m.x, m.y}, m.size, {235, 242, 255, 200});
        } else if (kind_ == 2) {
            const float flicker = 0.5f + 0.5f * std::sin(time_ * 6 + m.sway * 3);
            glow(ren, {m.x, m.y}, m.size * 5, with_alpha({255, 150, 70, 255}, 90 * flicker));
            fill_disc(ren, {m.x, m.y}, m.size, {255, 210, 150, static_cast<Uint8>(150 + 100 * flicker)});
        } else {
            fill_disc(ren, {m.x, m.y}, m.size, with_alpha(t.sun, 50 + 60 * (0.5f + 0.5f * std::sin(time_ + m.sway))));
        }
    }
}

}  // namespace kid::app
