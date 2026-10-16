#include "effects.hpp"

#include "scene.hpp"

#include <algorithm>
#include <cmath>

namespace kid::app {

namespace {

constexpr float kBitLife = 1.1f, kPointsLife = 1.2f;
constexpr double kTrailEvery = 0.05;  // seconds of flight between trail dots

}  // namespace

float Effects::random() {
    seed_ = seed_ * 1664525u + 1013904223u;
    return static_cast<float>(seed_ >> 8) / static_cast<float>(1 << 24);
}

void Effects::clear() {
    bits_.clear();
    points_.clear();
    trail_.clear();
    last_trail_.clear();
    pops_seen_ = shots_seen_ = 0;
    last_mark_ = -1;
    level_time_ = 0;
}

void Effects::bird_bonus(Vec2 at, int value) { points_.push_back({at, value, kPointsLife * 1.5f, color::kTarget}); }

void Effects::update(const Level& level, float dt) {
    if (level.sim_time < level_time_ || level.pops.size() < pops_seen_ || level.shots.size() < shots_seen_) clear();  // reset
    level_time_ = level.sim_time;

    // Each pop: a burst of bits in its colours, more for bigger things, and its points.
    for (; pops_seen_ < level.pops.size(); ++pops_seen_) {
        const Level::Pop& pop = level.pops[pops_seen_];
        const RoleColors c = pop.kind == Kind::Pig ? role_colors(Role::Pig) : body_colors(Role::Block, pop.material);
        const int count = 6 + static_cast<int>(pop.size * 14.0f);
        for (int i = 0; i < count; ++i) {
            const float a = random() * 6.2832f, speed = 1.5f + random() * 4.0f;
            const Vec2 v{std::cos(a) * speed, std::sin(a) * speed + 2.0f};
            const Vec2 offset = Vec2{random() - 0.5f, random() - 0.5f} * (pop.size * 1.4f);
            const float life = kBitLife * (0.6f + 0.4f * random());
            bits_.push_back({pop.pos + offset, v, random() * 3.0f, (random() - 0.5f) * 12.0f, 0.04f + random() * pop.size * 0.14f, life, life,
                             random() < 0.3f ? c.edge : c.fill});
        }
        points_.push_back({pop.pos + Vec2{0, pop.size + 0.2f}, pop.points, kPointsLife, pop.kind == Kind::Pig ? color::kGood : color::kText});
    }

    // The trail of the latest bird: a dot every so often while it is still flying (it stops once the
    // bird has slowed down). A new bird keeps the previous one's trail, fainter.
    if (level.shots.size() > shots_seen_) {
        shots_seen_ = level.shots.size();
        last_trail_ = std::move(trail_);
        trail_.clear();
        last_mark_ = -1;
    }
    if (!level.shots.empty()) {
        const int bird = level.shots.back();
        const Body& b = level.world.bodies[static_cast<size_t>(bird)];
        if (!level.popped(bird) && b.vel.length() > 2.0f && level.sim_time - last_mark_ >= kTrailEvery) {
            trail_.push_back(b.pos);
            last_mark_ = level.sim_time;
        }
    }

    for (Bit& bit : bits_) {
        bit.v += Vec2{0, -9.81f} * dt;
        bit.p += bit.v * dt;
        bit.angle += bit.spin * dt;
        bit.life -= dt;
        if (bit.p.y < kFloorTop + bit.size) {  // they settle on the floor rather than fall through it
            bit.p.y = kFloorTop + bit.size;
            bit.v = {bit.v.x * 0.5f, std::fabs(bit.v.y) * 0.25f};
        }
    }
    std::erase_if(bits_, [](const Bit& b) { return b.life <= 0; });
    for (Points& p : points_) {
        p.p.y += 0.9f * dt;
        p.life -= dt;
    }
    std::erase_if(points_, [](const Points& p) { return p.life <= 0; });
}

void Effects::draw_under(SDL_Renderer* ren) const {
    for (const Vec2 p : last_trail_) fill_disc(ren, to_screen(p), 2.0f, {200, 206, 222, 70});
    for (const Vec2 p : trail_) fill_disc(ren, to_screen(p), 2.5f, {220, 226, 240, 150});
}

void Effects::draw_over(SDL_Renderer* ren, const Level& level, float top_of_view) const {
    for (const Bit& b : bits_) {
        SDL_Color c = b.color;
        c.a = static_cast<Uint8>(std::clamp(b.life / (b.max_life * 0.4f), 0.0f, 1.0f) * static_cast<float>(c.a));
        const Vec2 ax{std::cos(b.angle) * b.size, std::sin(b.angle) * b.size}, ay{-ax.y, ax.x};
        const SDL_FPoint pts[4] = {to_screen(b.p - ax - ay), to_screen(b.p + ax - ay), to_screen(b.p + ax + ay), to_screen(b.p - ax + ay)};
        fill_polygon(ren, pts, 4, c);
    }
    for (const Points& p : points_) {
        SDL_Color c = p.color;
        c.a = static_cast<Uint8>(std::clamp(p.life / (kPointsLife * 0.5f), 0.0f, 1.0f) * 255.0f);
        char buf[16];
        SDL_snprintf(buf, sizeof(buf), "+%d", p.value);
        const SDL_FPoint s = to_screen(p.p);
        text_centered(ren, s.x, s.y, 2.0f, c, buf);
    }
    // A bird above the top of the view: a marker just under the bar, and how high it is.
    for (size_t i = 0; i < level.world.bodies.size(); ++i) {
        const Role r = level.role(i);
        if (level.shot_of(i) < 0 || r == Role::Hidden) continue;
        const Body& b = level.world.bodies[i];
        if (b.pos.y - b.shape.circle.radius < top_of_view) continue;
        const SDL_FPoint s = to_screen(b.pos);
        const float y = to_screen({0, top_of_view}).y + 6.0f;
        const SDL_FPoint tri[3] = {{s.x, y}, {s.x + 9.0f, y + 14.0f}, {s.x - 9.0f, y + 14.0f}};
        fill_polygon(ren, tri, 3, role_colors(r).fill);
        char buf[16];
        SDL_snprintf(buf, sizeof(buf), "%.0f m", static_cast<double>(b.pos.y - kFloorTop));
        text_centered(ren, s.x, y + 20.0f, 2.0f, role_colors(r).edge, buf);
    }
}

}  // namespace kid::app
