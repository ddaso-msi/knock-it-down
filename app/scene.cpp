#include "scene.hpp"

#include <algorithm>
#include <cmath>

namespace kid::app {

RoleColors role_colors(Role role) {
    switch (role) {
        case Role::Scenery: return {color::kStatic, color::kStaticEdge};
        case Role::Driven: return {{122, 98, 78, 255}, {255, 236, 200, 255}};  // loose wood, with a bright edge: it moves by itself
        case Role::Target: return {color::kTarget, color::kTargetEdge};
        case Role::Done: return {color::kDone, color::kDoneEdge};
        case Role::Keep: return {{150, 112, 222, 255}, {222, 204, 255, 255}};
        case Role::Lost: return {{150, 60, 60, 255}, color::kBad};
        case Role::Loose: return {{122, 98, 78, 255}, {206, 178, 148, 255}};  // plain wood: brown, nothing to do with you
        case Role::Ball: return {color::kTool, color::kToolEdge};
        case Role::Heavy: return {{40, 92, 160, 255}, {130, 180, 240, 255}};
        case Role::Blast: return {{226, 72, 96, 255}, {255, 176, 186, 255}};
        case Role::Hidden: return {{0, 0, 0, 0}, {0, 0, 0, 0}};
        case Role::Guide: return {{96, 104, 128, 255}, {190, 196, 214, 255}};
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

// Stone is darker and greyer, ice is pale and see-through, so a material reads before you read the help.
RoleColors with_material(RoleColors c, Material m) {
    if (m == Material::Stone) {
        c.fill = mix(c.fill, {70, 70, 76, 255}, 0.45f);
        c.edge = mix(c.edge, {200, 200, 200, 255}, 0.3f);
    } else if (m == Material::Ice) {
        c.fill = faded(mix(c.fill, {225, 245, 255, 255}, 0.55f), 190);
        c.edge = mix(c.edge, {255, 255, 255, 255}, 0.6f);
    }
    return c;
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

void draw_level(SDL_Renderer* ren, const Level& level, Vec2 pointer, bool pointer_active, bool show_contacts) {
    draw_grid(ren);
    Overlay marks;
    level.overlay(marks, pointer, pointer_active);

    const World& world = level.world;
    for (size_t i = 0; i < world.bodies.size(); ++i) {
        const Role r = level.role(i);
        if (r == Role::Hidden) continue;
        const RoleColors c = with_material(role_colors(r), level.material(i));
        draw_body(ren, world.bodies[i], c.fill, c.edge);
    }
    for (const phys::Joint& j : world.joints) draw_joint(ren, world, j);

    for (const Overlay::Line& l : marks.lines) {
        const SDL_Color c = role_colors(l.role).edge;
        if (l.dashed) {
            dashed_line(ren, l.a, l.b, faded(c, 170));
        } else {
            const SDL_FPoint a = to_screen(l.a), b = to_screen(l.b);
            set_color(ren, c);
            SDL_RenderLine(ren, a.x, a.y, b.x, b.y);
            SDL_RenderLine(ren, a.x, a.y + 1, b.x, b.y + 1);
        }
    }
    for (const Overlay::Ring& r : marks.rings) {
        const Uint8 alpha = static_cast<Uint8>(std::clamp(r.fade, 0.0f, 1.0f) * 255.0f);
        fill_disc(ren, to_screen(r.p), r.radius * kPixelsPerMeter, {255, 150, 90, static_cast<Uint8>(alpha / 5)});
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
    const Role r = a == Ammo::Heavy ? Role::Heavy : a == Ammo::Blast ? Role::Blast : Role::Ball;
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
