#include "level.hpp"

#include "objectives.hpp"

#include <phys/aabb.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace kid {

namespace {

constexpr double kFlashTime = 0.35;  // how long a blast's ring stays on screen

Body make_piece(const Piece& p) {
    const MaterialProps m = material_props(p.material);
    const phys::Shape shape = p.radius > 0 ? phys::Shape::make_circle(p.radius)
                                           : phys::Shape::make_polygon(phys::Polygon::box(p.half.x, p.half.y));
    Body b(shape, p.pos, p.angle, phys::BodyType::Dynamic, m.density);
    b.friction = m.friction;
    b.restitution = m.restitution;
    return b;
}

Body make_static(Vec2 pos, Vec2 half, Real angle = 0) {
    Body b(phys::Shape::make_polygon(phys::Polygon::box(half.x, half.y)), pos, angle, phys::BodyType::Static);
    b.restitution = 0;
    b.friction = 0.6f;
    return b;
}

}  // namespace

Level::Level(Layout layout) : layout_(std::move(layout)) { reset(); }

void Level::reset() {
    if (tried_) ++attempt;
    tried_ = false;
    status = Status::Playing;
    failure = Failure::None;
    sim_time = win_time = 0;
    last_shot_time_ = quiet_for_ = win_hold_ = 0;
    shots.clear();
    shot_ammo_.clear();
    flashes_.clear();
    live_blast = -1;
    build();
}

void Level::build() {
    // Always a new World: body indices are identities in this engine and there is no removing one.
    world = World{};
    targets.clear();
    keeps.clear();
    piece_of_.clear();
    watched_.clear();
    driven_.clear();
    drives_.clear();

    // The room: floor (top at kFloorTop), side walls, and a ceiling just out of view.
    const Real t = kFloorTop * 0.5f;
    world.add(make_static({kWorldW * 0.5f, t}, {kWorldW * 0.5f, t}));
    world.add(make_static({t, kWorldH * 0.5f}, {t, kWorldH * 0.5f}));
    world.add(make_static({kWorldW - t, kWorldH * 0.5f}, {t, kWorldH * 0.5f}));
    world.add(make_static({kWorldW * 0.5f, kWorldH + t}, {kWorldW * 0.5f, t}));
    // The launcher's pedestal: its top half a metre under the muzzle, clear of the biggest shot.
    const Real pedestal = (layout_.muzzle.y - 0.5f - kFloorTop) * 0.5f;
    if (pedestal > 0) world.add(make_static({layout_.muzzle.x, kFloorTop + pedestal}, {0.45f, pedestal}));
    for (const Wall& w : layout_.walls) world.add(make_static(w.pos, w.half, w.angle));
    piece_of_.assign(world.bodies.size(), -1);

    for (size_t i = 0; i < layout_.pieces.size(); ++i) {
        const Piece& p = layout_.pieces[i];
        const int body = world.add(make_piece(p));
        piece_of_.push_back(static_cast<int>(i));
        if (p.kind == Kind::Target) targets.push_back(body);
        if (p.kind == Kind::Keep) keeps.push_back(body);
    }
    const auto body_of = [&](int piece) { return static_cast<int>(piece_of_.size() - layout_.pieces.size()) + piece; };
    const auto drive = [&](int body) {
        world.bodies[static_cast<size_t>(body)].allow_sleep = false;
        driven_.push_back(body);
    };

    for (const Hinge& h : layout_.hinges) {
        const int b = body_of(h.piece);
        phys::Joint j = phys::Joint::revolute(world.bodies, -1, b, h.anchor);
        if (h.motor) {
            j.enable_motor = true;
            j.motor_speed = h.speed;
            j.max_motor = h.torque;
            drive(b);
        }
        world.add_joint(j);
    }
    for (const Rope& r : layout_.ropes) world.add_joint(phys::Joint::distance(world.bodies, -1, body_of(r.piece), r.anchor, r.attach));
    for (const Slider& s : layout_.sliders) {
        const int b = body_of(s.piece);
        const Vec2 origin = world.bodies[static_cast<size_t>(b)].pos;
        phys::Joint j = phys::Joint::prismatic(world.bodies, -1, b, origin, s.axis);
        j.enable_motor = true;
        j.max_motor = s.force;
        drives_.push_back({world.add_joint(j), b, origin, s});
        drive(b);
    }

    for (size_t i = 0; i < world.bodies.size(); ++i)
        if (world.bodies[i].type == phys::BodyType::Dynamic && std::find(driven_.begin(), driven_.end(), static_cast<int>(i)) == driven_.end())
            watched_.push_back(static_cast<int>(i));
    fallen = 0;
}

void Level::step() {
    // Sliders follow their swing: the motor is asked for the swing's velocity at the end of this step,
    // plus a little of whatever position error a shot has caused, so the swing never drifts.
    for (const Drive& d : drives_) {
        const double omega = 2 * static_cast<double>(phys::kPi) / static_cast<double>(d.slider.period);
        const double t = sim_time + static_cast<double>(kTimeStep);
        const Real want = d.slider.amplitude * static_cast<Real>(1 - std::cos(omega * t));
        const Real along = dot(world.bodies[static_cast<size_t>(d.body)].pos - d.origin, d.slider.axis);
        world.joints[static_cast<size_t>(d.joint)].motor_speed =
            d.slider.amplitude * static_cast<Real>(omega * std::sin(omega * t)) + (want - along) * 2.0f;
    }

    world.step(kTimeStep);
    sim_time += static_cast<double>(kTimeStep);

    if (live_blast >= 0)  // a blast goes off at the first thing it touches
        for (const phys::ContactPair& c : world.contacts())
            if (!c.dormant && c.manifold.count > 0 && (c.a == live_blast || c.b == live_blast)) {
                explode(live_blast);
                break;
            }
    std::erase_if(flashes_, [&](const Flash& f) { return sim_time - f.time > kFlashTime; });

    if (status != Status::Won) evaluate();
}

bool Level::is_down(int body) const {
    const int p = piece_of_[static_cast<size_t>(body)];
    return world.bodies[static_cast<size_t>(body)].pos.y < layout_.pieces[static_cast<size_t>(p)].fall_y;
}

void Level::evaluate() {
    fallen = static_cast<int>(std::count_if(targets.begin(), targets.end(), [&](int b) { return is_down(b); }));
    if (std::any_of(keeps.begin(), keeps.end(), [&](int b) { return is_down(b); })) {
        status = Status::Failed;
        failure = Failure::KeepLost;
    }
    if (failure == Failure::KeepLost) return;  // final: nothing turns this back into a win

    const bool quiet_now = quiet();
    quiet_for_ = quiet_now ? quiet_for_ + static_cast<double>(kTimeStep) : 0;

    if (fallen == static_cast<int>(targets.size())) {
        // A late block can still turn "out of shots" into a win.
        win_hold_ += static_cast<double>(kTimeStep);
        if (keeps.empty() || win_hold_ >= static_cast<double>(kWinHold) - 1e-9) {
            status = Status::Won;
            failure = Failure::None;
            win_time = sim_time;
        }
        return;
    }
    win_hold_ = 0;
    if (shots_left() == 0 && live_blast < 0 && status == Status::Playing) {
        const double since = since_last_shot();
        if ((quiet_for_ >= static_cast<double>(kQuietTime) && since >= static_cast<double>(kQuietTime)) ||
            since >= static_cast<double>(kSettleTime)) {
            status = Status::Failed;
            failure = Failure::OutOfShots;
        }
    }
}

bool Level::quiet() const { return all_slow(world, watched_); }

Vec2 Level::launch_velocity(Vec2 aim) const {
    const Vec2 d = aim - layout_.muzzle;
    const Real len = d.length();
    if (len < 1e-4f) return {kMinSpeed, 0};
    return d * (std::clamp(len * kSpeedPerMeter, kMinSpeed, kMaxSpeed) / len);
}

Vec2 Level::aim_for(Real angle, Real speed) const {
    const Real s = std::clamp(speed, kMinSpeed, kMaxSpeed);
    return layout_.muzzle + Vec2{std::cos(angle), std::sin(angle)} * (s / kSpeedPerMeter);
}

std::optional<Ammo> Level::next_ammo() const {
    if (shots_left() <= 0) return std::nullopt;
    return layout_.ammo[static_cast<size_t>(shots_used())];
}

bool Level::can_fire() const {
    if (status != Status::Playing || shots_left() <= 0 || live_blast >= 0) return false;
    return shots.empty() || since_last_shot() >= static_cast<double>(kReloadTime);
}

bool Level::fire(Vec2 aim) {
    if (!can_fire()) return false;
    const Ammo kind = *next_ammo();
    const AmmoProps a = ammo_props(kind);
    Body shot(phys::Shape::make_circle(a.radius), layout_.muzzle, 0, phys::BodyType::Dynamic, a.density);
    shot.restitution = a.restitution;
    shot.friction = a.friction;
    shot.vel = launch_velocity(aim);
    const int body = world.add(shot);
    piece_of_.push_back(-1);
    watched_.push_back(body);
    shots.push_back(body);
    shot_ammo_.push_back(kind);
    if (kind == Ammo::Blast) live_blast = body;
    last_shot_time_ = sim_time;
    quiet_for_ = 0;
    tried_ = true;
    return true;
}

bool Level::detonate() {
    if (live_blast < 0) return false;
    explode(live_blast);
    return true;
}

void Level::explode(int index) {
    Body& blast = world.bodies[static_cast<size_t>(index)];
    const Vec2 at = blast.pos;
    // Every dynamic body near the blast gets a kick away from it, strongest close in. Distance is to the
    // nearest point of the body's box, so a long plank is pushed when its end is close.
    for (size_t i = 0; i < world.bodies.size(); ++i) {
        Body& b = world.bodies[i];
        if (static_cast<int>(i) == index || b.type != phys::BodyType::Dynamic) continue;
        const phys::AABB box = phys::compute_aabb(b);
        const Vec2 nearest{std::clamp(at.x, box.lo.x, box.hi.x), std::clamp(at.y, box.lo.y, box.hi.y)};
        const Real d = distance(at, nearest);
        if (d >= kBlastRadius) continue;
        Vec2 dir = b.pos - at;
        dir = dir.length() > 1e-4f ? dir.normalized() : Vec2{0, 1};
        b.vel += dir * (kBlastImpulse * (1 - d / kBlastRadius) * b.inv_mass);
        b.wake();
    }
    // The shot itself is spent: it becomes an immovable body that touches nothing, out of the way.
    // (A body cannot be removed without renumbering the ones after it.)
    blast.type = phys::BodyType::Static;
    blast.inv_mass = blast.inv_inertia = 0;
    blast.vel = {};
    blast.w = 0;
    blast.category = blast.mask = 0;
    blast.pos = {-10, -10};
    std::erase(watched_, index);
    flashes_.push_back({at, sim_time});
    live_blast = -1;
    last_shot_time_ = sim_time;  // the settle clock starts from the explosion
}

int Level::stars() const {
    if (status != Status::Won) return 0;
    if (shots_used() <= layout_.par) return 3;
    return shots_used() == layout_.par + 1 ? 2 : 1;
}

Role Level::role(size_t body) const {
    const Body& b = world.bodies[body];
    for (size_t s = 0; s < shots.size(); ++s)
        if (shots[s] == static_cast<int>(body)) {
            if (b.mask == 0) return Role::Hidden;
            return shot_ammo_[s] == Ammo::Heavy ? Role::Heavy : shot_ammo_[s] == Ammo::Blast ? Role::Blast : Role::Ball;
        }
    if (std::find(driven_.begin(), driven_.end(), static_cast<int>(body)) != driven_.end()) return Role::Driven;
    const int p = body < piece_of_.size() ? piece_of_[body] : -1;
    if (p < 0) return Role::Scenery;
    switch (layout_.pieces[static_cast<size_t>(p)].kind) {
        case Kind::Target: return is_down(static_cast<int>(body)) ? Role::Done : Role::Target;
        case Kind::Keep: return is_down(static_cast<int>(body)) ? Role::Lost : Role::Keep;
        case Kind::Loose: return Role::Loose;
    }
    return Role::Loose;
}

Material Level::material(size_t body) const {
    const int p = body < piece_of_.size() ? piece_of_[body] : -1;
    return p < 0 ? Material::Wood : layout_.pieces[static_cast<size_t>(p)].material;
}

void Level::overlay(Overlay& out, Vec2 pointer, bool pointer_active) const {
    for (const FallLine& f : layout_.fall_lines) out.lines.push_back({{f.x0, f.y}, {f.x1, f.y}, Role::Target, true});
    for (const Flash& f : flashes_) {
        const float age = static_cast<float>((sim_time - f.time) / kFlashTime);
        out.rings.push_back({f.p, kBlastRadius * (0.3f + 0.7f * age), 1.0f - age});
    }

    // The launcher, with the next shot loaded while there is one.
    const Vec2 muzzle = layout_.muzzle;
    out.dots.push_back({muzzle, 0.4f, Role::Scenery});
    if (const std::optional<Ammo> next = next_ammo(); next && status == Status::Playing) {
        const Role r = *next == Ammo::Heavy ? Role::Heavy : *next == Ammo::Blast ? Role::Blast : Role::Ball;
        out.dots.push_back({muzzle, ammo_props(*next).radius, can_fire() ? r : Role::Guide});
    }
    if (!pointer_active || !can_fire()) return;

    // The same arithmetic the engine uses for a body in free flight, so the dots are where the shot goes.
    const Vec2 v0 = launch_velocity(pointer);
    Vec2 p = muzzle, v = v0;
    for (int i = 1; i <= 150; ++i) {
        v += world.gravity * kTimeStep;
        p += v * kTimeStep;
        if (p.y < kFloorTop || p.x < 0 || p.x > kWorldW) break;
        if (i % 5 == 0) out.dots.push_back({p, 0.04f, Role::Guide});
    }
    out.lines.push_back({muzzle, muzzle + v0.normalized() * 0.8f, Role::Ball, false});
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.0f m/s", static_cast<double>(v0.length()));
    out.labels.push_back({muzzle + Vec2{-0.7f, 1.0f}, buf, Role::Ball});
}

std::vector<std::string> Level::help() const {
    std::vector<std::string> lines = {
        "Move the mouse to aim: the dots show the path.",
        "Aim further from the launcher for more speed. Click to fire.",
        "Every orange block has to end up below its dashed line.",
    };
    for (const std::string& n : layout_.notes) lines.push_back(n);
    return lines;
}

std::string Level::hint() const {
    if (status == Status::Won) return "Solved.";
    if (status == Status::Failed)
        return failure == Failure::KeepLost ? "A purple block fell. R to try again." : "Out of shots. R to try again.";
    if (live_blast >= 0) return "Click to set it off, or let it hit something.";
    if (shots_left() == 0) return "Last shot away. Watch what it does.";
    if (!can_fire()) return "Reloading...";
    const Ammo next = *next_ammo();
    if (next == Ammo::Heavy) return "Next: a heavy shot. Aim, then click.";
    if (next == Ammo::Blast) return "Next: a blast. It goes off when it hits.";
    return "Aim with the mouse, click to fire.";
}

std::string Level::stats() const {
    char buf[96];
    int n = std::snprintf(buf, sizeof(buf), "Shots %d/%d   Down %d/%d", shots_left(), static_cast<int>(layout_.ammo.size()), fallen,
                          static_cast<int>(targets.size()));
    if (!keeps.empty()) {
        const int safe = static_cast<int>(std::count_if(keeps.begin(), keeps.end(), [&](int b) { return !is_down(b); }));
        std::snprintf(buf + n, sizeof(buf) - static_cast<size_t>(n), "   Keep %d/%d", safe, static_cast<int>(keeps.size()));
    }
    return buf;
}

std::string Level::result() const {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%d shot%s (par %d), %.1f s", shots_used(), shots_used() == 1 ? "" : "s", layout_.par, win_time);
    return buf;
}

}  // namespace kid
