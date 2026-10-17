#include "scene.hpp"

#include "effects.hpp"
#include "look.hpp"

#include <algorithm>
#include <cmath>
#include <vector>
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

void draw_grid(SDL_Renderer* ren) {  // a line every metre of the arena, wherever the camera is
    set_color(ren, color::kGrid);
    for (int x = 1; x < static_cast<int>(kWorldW); ++x) {
        const SDL_FPoint a = to_screen({static_cast<float>(x), 0}), b = to_screen({static_cast<float>(x), kWorldH});
        SDL_RenderLine(ren, a.x, a.y, b.x, b.y);
    }
    for (int y = 1; y <= static_cast<int>(kWorldH); ++y) {
        const SDL_FPoint a = to_screen({0, static_cast<float>(y)}), b = to_screen({kWorldW, static_cast<float>(y)});
        SDL_RenderLine(ren, a.x, a.y, b.x, b.y);
    }
}

namespace {


SDL_Color faded(SDL_Color c, Uint8 alpha) { return {c.r, c.g, c.b, alpha}; }

// Blocks are coloured by what they are made of, so a material reads before you read the help.
RoleColors with_material(RoleColors c, Material m) {
    if (m == Material::Stone) return {{108, 110, 120, 255}, {192, 194, 204, 255}};
    if (m == Material::Ice) return {{160, 210, 235, 170}, {232, 248, 255, 255}};
    return c;
}

}  // namespace

RoleColors body_colors(Role role, Material m) { return role == Role::Block ? with_material(role_colors(role), m) : role_colors(role); }

namespace {

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

void draw_level(SDL_Renderer* ren, const Level& level, std::optional<Vec2> pull, bool show_contacts, const Effects* fx, const Theme& theme) {
    draw_backdrop(ren, theme, level.sim_time);
    draw_ground(ren, theme);
    if (fx) fx->draw_under(ren);
    const Vec2 wobble = fx ? fx->sling_wobble() : Vec2{};
    Overlay marks;
    level.overlay(marks, pull, wobble, false);

    // What the pigs are looking at: the bird in flight, or the one in the sling.
    const World& world = level.world;
    Vec2 look_at = level.layout().pouch;
    if (!level.shots.empty() && !level.popped(level.shots.back())) look_at = world.bodies[static_cast<size_t>(level.shots.back())].pos;

    // Ropes behind what hangs on them.
    for (const phys::Joint& j : world.joints)
        if (j.type == phys::JointType::Distance)
            thick_line(ren, to_screen(j.world_anchor_a(world.bodies)), to_screen(j.world_anchor_b(world.bodies)), std::max(2.0f, to_pixels(0.04f)), kInk);

    for (size_t i = 0; i < world.bodies.size(); ++i) {
        const Role r = level.role(i);
        if (r == Role::Hidden || i == 0) continue;  // the floor is drawn as the ground
        const Body& b = world.bodies[i];
        const Real condition = level.condition(i);
        if (level.shot_of(i) >= 0) {  // a bird: stretched by its speed, squashed by its last hit
            const Real speed = b.vel.length();
            const Vec2 heading = speed > 0.5f ? b.vel * (1 / speed) : Vec2{1, 0};
            draw_bird(ren, b.pos, b.shape.circle.radius, heading, 1 + std::min<Real>(0.22f, speed / 90.0f), fx ? fx->squash(static_cast<int>(i)) : 0.0f,
                      level.layout().ammo[static_cast<size_t>(level.shot_of(i))], r == Role::Egg, theme);
            continue;
        }
        if (r == Role::Scenery) draw_rock(ren, b, theme);
        else if (r == Role::Pig) draw_pig(ren, b, *level.pig_kind(i), condition, look_at, level.sim_time, static_cast<unsigned>(i), theme);
        else draw_block(ren, b, level.material(i), condition, static_cast<unsigned>(i), theme);
    }
    for (const phys::Joint& j : world.joints)  // pins on top
        if (j.type == phys::JointType::Revolute) {
            const SDL_FPoint a = to_screen(j.world_anchor_a(world.bodies));
            fill_disc(ren, a, std::max(4.0f, to_pixels(0.1f)), kInk);
            fill_disc(ren, a, std::max(2.0f, to_pixels(0.04f)), theme.sun);
        }

    // The birds still to come, waiting their turn on the ground behind the sling, hopping now and then.
    {
        const std::vector<Ammo>& ammo = level.layout().ammo;
        int place = 0;
        Real x = level.layout().pouch.x - 0.3f;  // the right edge of the line
        for (size_t k = static_cast<size_t>(level.shots_used()) + 1; k < ammo.size(); ++k, ++place) {
            const Real r = ammo_props(ammo[k]).radius * 0.85f;  // a touch smaller: five have to fit beside the wall
            x -= r + 0.04f;
            const float phase = static_cast<float>(level.sim_time) * 2.3f + static_cast<float>(place) * 1.9f;
            const float hop = 0.25f * std::pow(std::max(0.0f, std::sin(phase)), 12.0f);
            const Vec2 at{x, kFloorTop + r + hop};
            x -= r + 0.04f;
            draw_bird(ren, at, r, {1, 0}, 1, 0, ammo[k], false, theme);
        }
    }

    // The sling, with the next bird in it.
    const Vec2 pouch = level.layout().pouch;
    std::optional<Vec2> held;
    Real held_radius = 0.25f;
    std::optional<Ammo> next = level.status == Status::Playing ? level.next_ammo() : std::nullopt;
    if (next) {
        const Vec2 p = pull ? level.pull_to(pouch + *pull) : Vec2{};
        held = level.launch_point(p);
        held_radius = ammo_props(*next).radius;
    }
    draw_sling(ren, pouch, held, held_radius, wobble, theme);
    if (!pull && held) draw_sling_front(ren, pouch, held, wobble);
    if (next) {
        const Vec2 p = pull ? level.pull_to(pouch + *pull) : Vec2{};
        const Vec2 aim = p.length() > 1e-3f ? p * (-1 / p.length()) : Vec2{1, 0};
        draw_bird(ren, *held, held_radius, aim, 1, 0, *next, false, theme);
    }
    // The front band crosses in front of the bird only while it is pulled; at rest it stays behind,
    // out of the bird's face.
    if (pull || !held) draw_sling_front(ren, pouch, held, wobble);

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
        const float fade = std::clamp(r.fade, 0.0f, 1.0f);
        if (r.radius > 1.0f) {  // a blast: the whole place lights up, then a hot white core
            glow(ren, to_screen(r.p), to_pixels(r.radius * (2.4f - fade)), {255, 170, 90, static_cast<Uint8>(230 * fade)});
            glow(ren, to_screen(r.p), to_pixels(r.radius * 0.6f), {255, 245, 220, static_cast<Uint8>(255 * fade)});
        } else {  // a pop or a power: a small flash, and the ring round a bird whose power is ready
            glow(ren, to_screen(r.p), to_pixels(r.radius * 1.5f), {255, 220, 170, static_cast<Uint8>(150 * fade)});
            ring(ren, r.p, r.radius, {255, 220, 170, static_cast<Uint8>(200 * fade)});
        }
    }
    for (const Overlay::Dot& d : marks.dots) {  // the path ahead, as a line of embers
        const SDL_FPoint p = to_screen(d.p);
        glow(ren, p, 9.0f, {255, 200, 140, 120});
        fill_disc(ren, p, 2.2f, {255, 232, 196, 255});
    }
    for (const Overlay::Label& l : marks.labels) {
        const SDL_FPoint p = to_screen(l.p);
        text(ren, p.x, p.y, 1.8f, {255, 232, 196, 230}, "%s", spaced(l.text).c_str());
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

void draw_ammo_icon(SDL_Renderer* ren, SDL_FPoint centre, Ammo a, bool spent, const Theme& theme) {
    const float radius = ammo_props(a).radius * kPixelsPerMeter * 0.8f;
    if (spent) {
        fill_disc(ren, centre, radius * 0.35f, {255, 232, 196, 60});
        return;
    }
    // In the dark interface a silhouette would vanish, so here a bird is its colour: a small body, a
    // beak, a dark eye.
    (void)theme;
    const SDL_Color c = bird_color(a);
    const SDL_FPoint beak[3] = {{centre.x + radius * 0.8f, centre.y - radius * 0.25f}, {centre.x + radius * 1.45f, centre.y + radius * 0.05f},
                                {centre.x + radius * 0.8f, centre.y + radius * 0.3f}};
    const SDL_FPoint tail[3] = {{centre.x - radius * 0.7f, centre.y - radius * 0.3f}, {centre.x - radius * 1.5f, centre.y - radius * 0.1f},
                                {centre.x - radius * 0.7f, centre.y + radius * 0.3f}};
    fill_polygon(ren, tail, 3, {c.r, c.g, c.b, 160});
    fill_disc(ren, centre, radius, {c.r, c.g, c.b, 220});
    fill_polygon(ren, beak, 3, {255, 214, 150, 230});
    fill_disc(ren, {centre.x + radius * 0.4f, centre.y - radius * 0.25f}, std::max(1.5f, radius * 0.16f), kInk);
}

}  // namespace kid::app
