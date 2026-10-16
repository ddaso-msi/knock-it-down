#include "level.hpp"

#include "objectives.hpp"

#include <phys/aabb.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace kid {

namespace {

constexpr double kFlashTime = 0.35;  // how long a blast's or a pop's ring stays on screen
constexpr Real kNoHealth = -1;       // health of a body that cannot be damaged

Body make_piece(const Piece& p) {
    const bool pig = p.kind == Kind::Pig;
    const MaterialProps m = material_props(p.material);
    const phys::Shape shape = p.radius > 0 ? phys::Shape::make_circle(p.radius)
                                           : phys::Shape::make_polygon(phys::Polygon::box(p.half.x, p.half.y));
    Body b(shape, p.pos, p.angle, phys::BodyType::Dynamic, pig ? 0.8f : m.density);
    b.friction = pig ? 0.6f : m.friction;
    b.restitution = pig ? 0.1f : m.restitution;
    return b;
}

Real starting_health(const Piece& p) {
    if (!p.breakable) return kNoHealth;
    if (p.kind == Kind::Pig) return pig_props(p.pig).health;
    const Real area = p.radius > 0 ? phys::kPi * p.radius * p.radius : 4 * p.half.x * p.half.y;
    return material_props(p.material).toughness * area;
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
    sim_time = win_time = 0;
    last_shot_time_ = quiet_for_ = 0;
    score = 0;
    shots.clear();
    shot_ammo_.clear();
    flashes_.clear();
    live_blast = -1;
    build();
}

void Level::build() {
    // Always a new World: body indices are identities in this engine and there is no removing one.
    world = World{};
    pigs.clear();
    blocks.clear();
    piece_of_.clear();
    watched_.clear();
    driven_.clear();
    drives_.clear();

    // The room: floor (top at kFloorTop), side walls, and a ceiling just out of view. The slingshot is
    // only drawn: nothing in the world stands where a bird is pulled back to.
    const Real t = kFloorTop * 0.5f;
    world.add(make_static({kWorldW * 0.5f, t}, {kWorldW * 0.5f, t}));
    world.add(make_static({t, kWorldH * 0.5f}, {t, kWorldH * 0.5f}));
    world.add(make_static({kWorldW - t, kWorldH * 0.5f}, {t, kWorldH * 0.5f}));
    world.add(make_static({kWorldW * 0.5f, kWorldH + t}, {kWorldW * 0.5f, t}));
    for (const Wall& w : layout_.walls) world.add(make_static(w.pos, w.half, w.angle));
    piece_of_.assign(world.bodies.size(), -1);
    health.assign(world.bodies.size(), kNoHealth);

    for (size_t i = 0; i < layout_.pieces.size(); ++i) {
        const Piece& p = layout_.pieces[i];
        const int body = world.add(make_piece(p));
        piece_of_.push_back(static_cast<int>(i));
        health.push_back(starting_health(p));
        (p.kind == Kind::Pig ? pigs : blocks).push_back(body);
    }
    max_health_ = health;
    pigs_left = static_cast<int>(pigs.size());
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

    // How fast two surfaces were closing is judged from the velocities before this step's solve: after
    // it, every contact has been stopped.
    std::vector<Vec2> vel(world.bodies.size());
    std::vector<Real> spin(world.bodies.size());
    for (size_t i = 0; i < world.bodies.size(); ++i) {
        vel[i] = world.bodies[i].vel;
        spin[i] = world.bodies[i].w;
    }
    world.step(kTimeStep);
    sim_time += static_cast<double>(kTimeStep);

    if (live_blast >= 0)  // a blast goes off at the first thing it touches
        for (const phys::ContactPair& c : world.contacts())
            if (!c.dormant && c.manifold.count > 0 && (c.a == live_blast || c.b == live_blast)) {
                explode(live_blast);
                break;
            }
    apply_damage(vel, spin);
    roll_drag();
    std::erase_if(flashes_, [&](const Flash& f) { return sim_time - f.time > kFlashTime; });

    if (status != Status::Won) evaluate();
}

void Level::apply_damage(const std::vector<Vec2>& vel, const std::vector<Real>& spin) {
    // A contact hurts both bodies by the impulse the solver needed to stop it, but only if they were
    // coming together faster than kDamageSpeed. That impulse is what a real impact delivers, so a heavy
    // shot hurts more than a light one at the same speed, and a block hurts what it lands on.
    for (const phys::ContactPair& c : world.contacts()) {
        if (c.dormant || c.manifold.count == 0) continue;
        const size_t a = static_cast<size_t>(c.a), b = static_cast<size_t>(c.b);
        if (a >= vel.size() || b >= vel.size() || (health[a] == kNoHealth && health[b] == kNoHealth)) continue;
        Real impulse = 0;
        for (int k = 0; k < c.manifold.count; ++k) {
            const phys::ContactPoint& p = c.manifold.points[k];
            const Vec2 va = vel[a] + cross(spin[a], p.point - world.bodies[a].pos);
            const Vec2 vb = vel[b] + cross(spin[b], p.point - world.bodies[b].pos);
            if (dot(va - vb, c.manifold.normal) > kDamageSpeed) impulse += p.normal_impulse;
        }
        if (impulse <= 0) continue;
        hurt(c.a, impulse);
        hurt(c.b, impulse);
    }
    for (size_t i = 0; i < health.size(); ++i)
        if (health[i] != kNoHealth && health[i] <= 0 && !popped(static_cast<int>(i))) pop(static_cast<int>(i));
}

void Level::roll_drag() {
    // Slows the spin directly; friction then brings the rolling speed down with it on the next step.
    std::vector<std::uint8_t> touching(world.bodies.size(), 0);
    for (const phys::ContactPair& c : world.contacts())
        if (!c.dormant && c.manifold.count > 0) touching[static_cast<size_t>(c.a)] = touching[static_cast<size_t>(c.b)] = 1;
    const Real drop = kRollDrag * kTimeStep;
    for (size_t i = 0; i < world.bodies.size(); ++i) {
        Body& b = world.bodies[i];
        if (!touching[i] || b.type != phys::BodyType::Dynamic || b.shape.type != phys::Shape::Type::Circle || !b.awake) continue;
        b.w = std::fabs(b.w) <= drop ? 0 : b.w - std::copysign(drop, b.w);
    }
}

void Level::hurt(int body, Real amount) {
    Real& h = health[static_cast<size_t>(body)];
    if (h == kNoHealth || popped(body)) return;
    h = std::max<Real>(0, h - amount);
}

void Level::pop(int body) {
    const int p = piece_of_[static_cast<size_t>(body)];
    const Piece& piece = layout_.pieces[static_cast<size_t>(p)];
    if (piece.kind == Kind::Pig) {
        score += pig_props(piece.pig).points;
        --pigs_left;
    } else {
        score += material_props(piece.material).points;
    }
    flashes_.push_back({world.bodies[static_cast<size_t>(body)].pos, sim_time, piece.kind == Kind::Pig ? 0.9f : 0.5f});
    remove(body);
}

void Level::remove(int index) {
    // A body cannot be removed without renumbering the ones after it, so it becomes an immovable body
    // that touches nothing, parked out of the way, and anything jointed to it lets go.
    Body& b = world.bodies[static_cast<size_t>(index)];
    b.type = phys::BodyType::Static;
    b.inv_mass = b.inv_inertia = 0;
    b.vel = {};
    b.w = 0;
    b.category = b.mask = 0;
    b.pos = {-10, -10};
    std::erase(watched_, index);
    for (int j = static_cast<int>(world.joints.size()) - 1; j >= 0; --j) {
        const phys::Joint& joint = world.joints[static_cast<size_t>(j)];
        if (joint.a != index && joint.b != index) continue;
        world.remove_joint(j);
        std::erase_if(drives_, [&](const Drive& d) { return d.joint == j; });
        for (Drive& d : drives_)
            if (d.joint > j) --d.joint;
    }
}

void Level::evaluate() {
    if (pigs_left == 0) {
        // A late pop can still turn "out of birds" into a win.
        status = Status::Won;
        win_time = sim_time;
        score += kBirdBonus * shots_left();
        return;
    }
    const bool quiet_now = quiet();
    quiet_for_ = quiet_now ? quiet_for_ + static_cast<double>(kTimeStep) : 0;
    if (shots_left() == 0 && live_blast < 0 && status == Status::Playing) {
        const double since = since_last_shot();
        if ((quiet_for_ >= static_cast<double>(kQuietTime) && since >= static_cast<double>(kQuietTime)) ||
            since >= static_cast<double>(kSettleTime))
            status = Status::Failed;
    }
}

bool Level::quiet() const { return all_slow(world, watched_); }

Vec2 Level::pull_to(Vec2 pointer) const {
    Vec2 d = pointer - layout_.pouch;
    if (d.length() > kMaxPull) d = d.normalized() * kMaxPull;
    // Keep the biggest bird clear of the floor and the walls wherever it is held.
    constexpr Real kClear = 0.36f;
    d.y = std::max(d.y, kFloorTop + kClear - layout_.pouch.y);
    d.x = std::clamp(d.x, kFloorTop + kClear - layout_.pouch.x, kWorldW - kFloorTop - kClear - layout_.pouch.x);
    return d;
}

Vec2 Level::pull_for(Real angle, Real speed) const {
    const Real s = std::clamp(speed, kMinPull * kPullSpeed, kMaxPull * kPullSpeed);
    return pull_to(layout_.pouch - Vec2{std::cos(angle), std::sin(angle)} * (s / kPullSpeed));
}

std::optional<Ammo> Level::next_ammo() const {
    if (shots_left() <= 0) return std::nullopt;
    return layout_.ammo[static_cast<size_t>(shots_used())];
}

bool Level::can_fire() const {
    if (status != Status::Playing || shots_left() <= 0 || live_blast >= 0) return false;
    return shots.empty() || since_last_shot() >= static_cast<double>(kReloadTime);
}

bool Level::fire(Vec2 pull) {
    pull = pull_to(layout_.pouch + pull);
    if (!can_fire() || pull.length() < kMinPull) return false;
    const Ammo kind = *next_ammo();
    const AmmoProps a = ammo_props(kind);
    Body shot(phys::Shape::make_circle(a.radius), launch_point(pull), 0, phys::BodyType::Dynamic, a.density);
    shot.restitution = a.restitution;
    shot.friction = a.friction;
    shot.vel = launch_velocity(pull);
    const int body = world.add(shot);
    piece_of_.push_back(-1);
    health.push_back(kNoHealth);
    max_health_.push_back(kNoHealth);
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
    const Vec2 at = world.bodies[static_cast<size_t>(index)].pos;
    // Every dynamic body near the blast gets a kick away from it and some damage, both strongest close
    // in. Distance is to the nearest point of the body's box, so a long plank is hit when its end is close.
    for (size_t i = 0; i < world.bodies.size(); ++i) {
        Body& b = world.bodies[i];
        if (static_cast<int>(i) == index || b.type != phys::BodyType::Dynamic) continue;
        const phys::AABB box = phys::compute_aabb(b);
        const Vec2 nearest{std::clamp(at.x, box.lo.x, box.hi.x), std::clamp(at.y, box.lo.y, box.hi.y)};
        const Real d = distance(at, nearest);
        if (d >= kBlastRadius) continue;
        const Real strength = 1 - d / kBlastRadius;
        Vec2 dir = b.pos - at;
        dir = dir.length() > 1e-4f ? dir.normalized() : Vec2{0, 1};
        b.vel += dir * (kBlastImpulse * strength * b.inv_mass);
        b.wake();
        hurt(static_cast<int>(i), kBlastDamage * strength);
    }
    remove(index);  // the shot itself is spent
    flashes_.push_back({at, sim_time, kBlastRadius});
    live_blast = -1;
    last_shot_time_ = sim_time;  // the settle clock starts from the explosion
    for (size_t i = 0; i < health.size(); ++i)
        if (health[i] != kNoHealth && health[i] <= 0 && !popped(static_cast<int>(i))) pop(static_cast<int>(i));
}

int Level::stars() const {
    if (status != Status::Won) return 0;
    if (shots_used() <= layout_.par) return 3;
    return shots_used() == layout_.par + 1 ? 2 : 1;
}

Role Level::role(size_t body) const {
    if (popped(static_cast<int>(body))) return Role::Hidden;
    for (size_t s = 0; s < shots.size(); ++s)
        if (shots[s] == static_cast<int>(body))
            return shot_ammo_[s] == Ammo::Heavy ? Role::Heavy : shot_ammo_[s] == Ammo::Blast ? Role::Blast : Role::Ball;
    if (std::find(driven_.begin(), driven_.end(), static_cast<int>(body)) != driven_.end()) return Role::Driven;
    const int p = body < piece_of_.size() ? piece_of_[body] : -1;
    if (p < 0) return Role::Scenery;
    return layout_.pieces[static_cast<size_t>(p)].kind == Kind::Pig ? Role::Pig : Role::Block;
}

Material Level::material(size_t body) const {
    const int p = body < piece_of_.size() ? piece_of_[body] : -1;
    return p < 0 ? Material::Wood : layout_.pieces[static_cast<size_t>(p)].material;
}

Real Level::condition(size_t body) const {
    if (health[body] == kNoHealth || max_health_[body] <= 0) return 1;
    return health[body] / max_health_[body];
}

std::optional<Pig> Level::pig_kind(size_t body) const {
    const int p = body < piece_of_.size() ? piece_of_[body] : -1;
    if (p < 0 || layout_.pieces[static_cast<size_t>(p)].kind != Kind::Pig) return std::nullopt;
    return layout_.pieces[static_cast<size_t>(p)].pig;
}

void Level::overlay(Overlay& out, std::optional<Vec2> pull) const {
    for (const Flash& f : flashes_) {
        const float age = static_cast<float>((sim_time - f.time) / kFlashTime);
        out.rings.push_back({f.p, f.radius * (0.3f + 0.7f * age), 1.0f - age});
    }

    // The slingshot: a post and a fork, with bands to the bird (or to each other when it is empty).
    const Vec2 pouch = layout_.pouch;
    const Vec2 fork = pouch + Vec2{0, -0.45f}, back = pouch + Vec2{-0.22f, 0.12f}, front = pouch + Vec2{0.22f, 0.12f};
    out.lines.push_back({{pouch.x, kFloorTop}, fork, Role::Band, false});
    out.lines.push_back({fork, back, Role::Band, false});
    out.lines.push_back({fork, front, Role::Band, false});
    const std::optional<Ammo> next = status == Status::Playing ? next_ammo() : std::nullopt;
    const Vec2 held = pull ? pull_to(pouch + *pull) : Vec2{};
    const Vec2 bird = launch_point(held);
    if (next) {
        const Role r = *next == Ammo::Heavy ? Role::Heavy : *next == Ammo::Blast ? Role::Blast : Role::Ball;
        out.lines.push_back({back, bird, Role::Guide, false});
        out.dots.push_back({bird, ammo_props(*next).radius, can_fire() ? r : Role::Guide});
        out.lines.push_back({front, bird, Role::Guide, false});
    } else {
        out.lines.push_back({back, front, Role::Guide, false});
    }
    if (!pull || !can_fire() || held.length() < kMinPull) return;

    // The same arithmetic the engine uses for a body in free flight, so the dots are where the bird goes.
    const Vec2 v0 = launch_velocity(held);
    Vec2 p = bird, v = v0;
    for (int i = 1; i <= 150; ++i) {
        v += world.gravity * kTimeStep;
        p += v * kTimeStep;
        if (p.y < kFloorTop || p.x < 0 || p.x > kWorldW) break;
        if (i % 5 == 0) out.dots.push_back({p, 0.04f, Role::Guide});
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.0f m/s", static_cast<double>(v0.length()));
    out.labels.push_back({pouch + Vec2{-0.7f, 1.2f}, buf, Role::Ball});
}

std::vector<std::string> Level::help() const {
    std::vector<std::string> lines = {
        "Press near the sling, drag the bird back, and let go. The dots show the path.",
        "The further you pull, the faster it flies.",
        "Hard hits break blocks and pop pigs. Pop every pig to win.",
        "Ice breaks easily, wood takes a knock, stone takes a beating.",
    };
    for (const std::string& n : layout_.notes) lines.push_back(n);
    return lines;
}

std::string Level::hint() const {
    if (status == Status::Won) return "Solved.";
    if (status == Status::Failed) return "Out of birds. R to try again.";
    if (live_blast >= 0) return "Click to set it off, or let it hit something.";
    if (shots_left() == 0) return "Last bird away. Watch what it does.";
    if (!can_fire()) return "Reloading...";
    const Ammo next = *next_ammo();
    if (next == Ammo::Heavy) return "Next: a heavy bird. Drag it back and let go.";
    if (next == Ammo::Blast) return "Next: a blast bird. It goes off when it hits.";
    return "Drag the bird back from the sling and let go.";
}

std::string Level::stats() const {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "Birds %d/%d   Pigs %d/%d   Score %d", shots_left(), static_cast<int>(layout_.ammo.size()), pigs_left,
                  static_cast<int>(pigs.size()), score);
    return buf;
}

std::string Level::result() const {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "Score %d: %d bird%s (par %d), %.1f s", score, shots_used(), shots_used() == 1 ? "" : "s", layout_.par,
                  win_time);
    return buf;
}

}  // namespace kid
