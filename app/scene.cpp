#include "scene.hpp"

#include "effects.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace kid::app {

RoleColors role_colors(Role role) {
    switch (role) {
        case Role::Scenery: return {color::kStatic, color::kStaticEdge};
        case Role::Driven: return {{150, 110, 70, 255}, {255, 236, 200, 255}};  // wood with a bright edge: it moves by itself
        case Role::Block: return {{150, 110, 70, 255}, {222, 182, 130, 255}};   // wood; with_material() for the rest
        case Role::Pig: return {{112, 188, 92, 255}, {204, 242, 176, 255}};
        case Role::Ball: return {color::kTool, color::kToolEdge};
        case Role::Heavy: return {{40, 92, 160, 255}, {130, 180, 240, 255}};
        case Role::Blast: return {{226, 72, 96, 255}, {255, 176, 186, 255}};
        case Role::Split: return {{64, 196, 200, 255}, {186, 246, 246, 255}};
        case Role::Dart: return {{232, 196, 64, 255}, {255, 238, 170, 255}};
        case Role::Dropper: return {{228, 226, 236, 255}, {255, 255, 255, 255}};
        case Role::Egg: return {{246, 240, 214, 255}, {200, 190, 150, 255}};
        case Role::Hidden: return {{0, 0, 0, 0}, {0, 0, 0, 0}};
        case Role::Guide: return {{96, 104, 128, 255}, {190, 196, 214, 255}};
        case Role::Band: return {{120, 82, 52, 255}, {168, 120, 78, 255}};
    }
    return {color::kStatic, color::kStaticEdge};
}

void draw_grid(SDL_Renderer* ren) {
    set_color(ren, color::kGrid);
    for (int x = 1; x < kWidth / static_cast<int>(kPixelsPerMeter); ++x) {
        const float sx = static_cast<float>(x) * kPixelsPerMeter;
        SDL_RenderLine(ren, sx, 0, sx, static_cast<float>(kHeight));
    }
    for (int y = 1; y <= kHeight / static_cast<int>(kPixelsPerMeter); ++y) {
        const SDL_FPoint p = to_screen({0, static_cast<float>(y)});
        SDL_RenderLine(ren, 0, p.y, static_cast<float>(kWidth), p.y);
    }
}

namespace {

void draw_joint(SDL_Renderer* ren, const World& world, const phys::Joint& j) {
    const SDL_FPoint a = to_screen(j.world_anchor_a(world.bodies)), b = to_screen(j.world_anchor_b(world.bodies));
    if (j.type == phys::JointType::Distance) {  // a rope, with its fixed end marked
        set_color(ren, color::kText);
        SDL_RenderLine(ren, a.x, a.y, b.x, b.y);
        if (j.a < 0) fill_disc(ren, a, 5.0f, color::kText);
    } else if (j.type == phys::JointType::Revolute) {  // a pin through the piece
        fill_disc(ren, a, 6.0f, color::kText);
        fill_disc(ren, a, 3.0f, color::kBackground);
    } else if (j.type == phys::JointType::Prismatic) {  // a rail: drawn by the level as its fall line's companion
        fill_disc(ren, b, 4.0f, color::kText);
    }
}

SDL_Color faded(SDL_Color c, Uint8 alpha) { return {c.r, c.g, c.b, alpha}; }
SDL_Color mix(SDL_Color a, SDL_Color b, float t) {
    const auto m = [t](Uint8 x, Uint8 y) { return static_cast<Uint8>(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t); };
    return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), a.a};
}

// Blocks are coloured by what they are made of, so a material reads before you read the help.
RoleColors with_material(RoleColors c, Material m) {
    if (m == Material::Stone) return {{108, 110, 120, 255}, {192, 194, 204, 255}};
    if (m == Material::Ice) return {{160, 210, 235, 170}, {232, 248, 255, 255}};
    return c;
}

}  // namespace

RoleColors body_colors(Role role, Material m) { return role == Role::Block ? with_material(role_colors(role), m) : role_colors(role); }

namespace {

// Lines across a damaged body: one below two thirds of its health, two below a third.
void draw_cracks(SDL_Renderer* ren, const Body& b, Real condition) {
    if (condition > 0.66f) return;
    const Real r = b.shape.type == phys::Shape::Type::Circle ? b.shape.circle.radius * 0.8f
                                                             : std::min(b.shape.polygon.vertices[2].x, b.shape.polygon.vertices[2].y) * 1.6f;
    const auto at = [&](Real x, Real y) { return to_screen(b.pos + rotate(b.q, Vec2{x, y} * r)); };
    set_color(ren, {40, 30, 24, 230});
    const SDL_FPoint c1[] = {at(-0.6f, 0.5f), at(-0.1f, 0.05f), at(0.15f, 0.3f), at(0.6f, -0.2f)};
    SDL_RenderLines(ren, c1, 4);
    if (condition > 0.33f) return;
    const SDL_FPoint c2[] = {at(-0.4f, -0.6f), at(0.0f, -0.15f), at(0.45f, -0.5f)};
    SDL_RenderLines(ren, c2, 3);
}

// A pig: the body is drawn as a disc already; this adds a face that turns with it, and a helmet.
void draw_pig_face(SDL_Renderer* ren, const Body& b, Pig kind) {
    const Real r = b.shape.circle.radius;
    const auto at = [&](Real x, Real y) { return to_screen(b.pos + rotate(b.q, Vec2{x, y} * r)); };
    const float px = r * kPixelsPerMeter;
    if (kind == Pig::Helmet) {  // a cap over the top third
        SDL_FPoint cap[18];
        for (int i = 0; i < 17; ++i) {
            const Real a = phys::kPi * (0.12f + 0.76f * static_cast<Real>(i) / 16.0f);
            cap[i] = at(1.08f * std::cos(a), 1.08f * std::sin(a));
        }
        cap[17] = cap[0];
        fill_polygon(ren, cap, 17, {150, 154, 166, 255});
        set_color(ren, {214, 218, 228, 255});
        SDL_RenderLines(ren, cap, 18);
    }
    for (Real side : {-1.0f, 1.0f}) {
        fill_disc(ren, at(0.38f * side, 0.28f), px * 0.22f, {245, 248, 240, 255});
        fill_disc(ren, at(0.42f * side, 0.26f), px * 0.1f, {20, 24, 20, 255});
    }
    fill_disc(ren, at(0, -0.22f), px * 0.3f, {150, 214, 124, 255});
    for (Real side : {-1.0f, 1.0f}) fill_disc(ren, at(0.12f * side, -0.22f), px * 0.07f, {50, 90, 40, 255});
}

void ring(SDL_Renderer* ren, Vec2 centre, Real radius, SDL_Color c) {
    constexpr int kSides = 48;
    SDL_FPoint pts[kSides + 1];
    for (int i = 0; i <= kSides; ++i) {
        const float a = 2.0f * phys::kPi * static_cast<float>(i) / kSides;
        pts[i] = to_screen(centre + Vec2{std::cos(a), std::sin(a)} * radius);
    }
    set_color(ren, c);
    SDL_RenderLines(ren, pts, kSides + 1);
}

}  // namespace

void draw_level(SDL_Renderer* ren, const Level& level, std::optional<Vec2> pull, bool show_contacts, const Effects* fx) {
    draw_grid(ren);
    if (fx) fx->draw_under(ren);
    Overlay marks;
    level.overlay(marks, pull);

    const World& world = level.world;
    for (size_t i = 0; i < world.bodies.size(); ++i) {
        const Role r = level.role(i);
        if (r == Role::Hidden) continue;
        RoleColors c = body_colors(r, level.material(i));
        const Real condition = level.condition(i);
        if (condition < 0.34f) c.fill = mix(c.fill, {50, 40, 34, c.fill.a}, 0.35f);  // nearly gone: darker
        draw_body(ren, world.bodies[i], c.fill, c.edge);
        if (r == Role::Pig) draw_pig_face(ren, world.bodies[i], *level.pig_kind(i));
        draw_cracks(ren, world.bodies[i], condition);
    }
    for (const phys::Joint& j : world.joints) draw_joint(ren, world, j);

    for (const Overlay::Line& l : marks.lines) {
        const SDL_Color c = role_colors(l.role).edge;
        if (l.dashed) {
            dashed_line(ren, l.a, l.b, faded(c, 170));
        } else {
            const SDL_FPoint a = to_screen(l.a), b = to_screen(l.b);
            set_color(ren, c);
            const int width = l.role == Role::Band ? 6 : l.role == Role::Guide ? 3 : 2;  // the sling's wood, its rubber, the rest
            for (int k = 0; k < width; ++k) {
                const float o = static_cast<float>(k) - static_cast<float>(width - 1) * 0.5f;
                SDL_RenderLine(ren, a.x + o, a.y, b.x + o, b.y);
            }
        }
    }
    for (const Overlay::Ring& r : marks.rings) {
        const Uint8 alpha = static_cast<Uint8>(std::clamp(r.fade, 0.0f, 1.0f) * 255.0f);
        if (r.radius > 1.0f) fill_disc(ren, to_screen(r.p), r.radius * kPixelsPerMeter, {255, 150, 90, static_cast<Uint8>(alpha / 5)});  // blasts only
        ring(ren, r.p, r.radius, {255, 200, 120, alpha});
        ring(ren, r.p, r.radius * 0.97f, {255, 120, 90, alpha});
    }
    for (const Overlay::Dot& d : marks.dots) {
        const RoleColors c = role_colors(d.role);
        const float r = d.radius * kPixelsPerMeter;
        if (r > 6.0f) fill_disc(ren, to_screen(d.p), r + 2.0f, c.edge);
        fill_disc(ren, to_screen(d.p), r, r > 6.0f ? c.fill : c.edge);
    }
    for (const Overlay::Label& l : marks.labels) {
        const SDL_FPoint p = to_screen(l.p);
        text(ren, p.x, p.y, 2.0f, role_colors(l.role).edge, "%s", l.text.c_str());
    }

    if (show_contacts)
        for (const phys::ContactPair& c : world.contacts())
            for (int k = 0; k < c.manifold.count; ++k) {
                const SDL_FPoint p = to_screen(c.manifold.points[k].point);
                const SDL_FPoint tip = to_screen(c.manifold.points[k].point + c.manifold.normal * 0.3f);
                set_color(ren, color::kGood);
                SDL_RenderLine(ren, p.x, p.y, tip.x, tip.y);
                fill_rect(ren, {p.x - 2.0f, p.y - 2.0f, 4.0f, 4.0f}, {255, 240, 150, 255});
            }
}

void draw_ammo_icon(SDL_Renderer* ren, SDL_FPoint centre, Ammo a, bool spent) {
    const Role r = role_of(a);
    const RoleColors c = role_colors(r);
    const float radius = ammo_props(a).radius * kPixelsPerMeter * 0.8f;
    if (spent) {
        fill_disc(ren, centre, radius, {50, 56, 72, 255});
        return;
    }
    fill_disc(ren, centre, radius + 2.0f, c.edge);
    fill_disc(ren, centre, radius, c.fill);
}

}  // namespace kid::app
