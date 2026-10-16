// The rules, on small hand-built layouts: the sling, damage and popping, winning and losing, blasts, motors.
#include "test.hpp"

#include <level.hpp>

#include <phys/collision.hpp>

#include <algorithm>
#include <cmath>

using namespace kid;
using namespace phys;

namespace {

const Vec2 kBackwards{1.5f, -0.3f};  // a pull that fires into the wall behind the sling: a shot that does nothing

void run(Level& g, double seconds) {
    const long steps = std::lround(seconds / static_cast<double>(kTimeStep));
    for (long i = 0; i < steps; ++i) g.step();
}

// One small pig on the floor in the far corner, out of everything's way (a level with no pigs left
// is already won), and nothing else. It is piece 0.
Layout one_pig(std::vector<Ammo> ammo = {Ammo::Ball, Ammo::Ball, Ammo::Ball}) {
    Layout l;
    l.name = "test";
    l.objective = "test";
    l.ammo = std::move(ammo);
    l.par = 1;
    pig(l, Pig::Small, 18.8f, kFloorTop);
    return l;
}
Layout empty_level(std::vector<Ammo> ammo = {Ammo::Ball, Ammo::Ball, Ammo::Ball}) { return one_pig(std::move(ammo)); }

int body_of_piece(const Level& g, int piece) {  // the room's four walls, the layout's walls, then the pieces
    return 4 + static_cast<int>(g.layout().walls.size()) + piece;
}

// Moves a body by hand (as if something had knocked it there), with this velocity, and wakes it.
void put(Level& g, int body, Vec2 p, Vec2 v = {}) {
    g.world.bodies[static_cast<size_t>(body)].pos = p;
    g.world.bodies[static_cast<size_t>(body)].vel = v;
    g.world.wake(body);
}

Real health_of(const Level& g, int body) { return g.health[static_cast<size_t>(body)]; }

bool same_state(const Level& a, const Level& b) {
    if (a.world.bodies.size() != b.world.bodies.size()) return false;
    for (size_t i = 0; i < a.world.bodies.size(); ++i) {
        const Body &p = a.world.bodies[i], &q = b.world.bodies[i];
        if (p.pos.x != q.pos.x || p.pos.y != q.pos.y || p.angle != q.angle || p.vel.x != q.vel.x || p.vel.y != q.vel.y || p.w != q.w) return false;
    }
    return a.status == b.status && a.pigs_left == b.pigs_left && a.sim_time == b.sim_time && a.shots == b.shots && a.health == b.health &&
           a.score == b.score;
}

}  // namespace

// ---- the sling ----

TEST(a_pull_is_clamped_kept_off_the_floor_and_fires_the_opposite_way) {
    const Level g(one_pig());
    const Vec2 pouch = g.layout().pouch;
    CHECK_NEAR(g.pull_to(pouch + Vec2{-30.0f, 0}).length(), Level::kMaxPull, 1e-5);
    Layout low = one_pig();
    low.pouch = {2.5f, 1.2f};  // a sling close to the ground: a full pull downwards would be under the floor
    const Level h(low);
    const Vec2 down = h.pull_to(low.pouch + Vec2{-0.3f, -5.0f});
    CHECK(h.launch_point(down).y - ammo_props(Ammo::Heavy).radius > kFloorTop);
    CHECK(down.length() > Level::kMinPull);
    const Vec2 pull{-1.0f, -0.5f};
    CHECK(g.pull_to(pouch + pull).x == pull.x && g.pull_to(pouch + pull).y == pull.y);  // in range: unchanged
    CHECK(g.launch_point(pull).x == pouch.x + pull.x && g.launch_point(pull).y == pouch.y + pull.y);
    const Vec2 v = g.launch_velocity(pull);
    CHECK_NEAR(v.x, 1.0 * Level::kPullSpeed, 1e-5);
    CHECK_NEAR(v.y, 0.5 * Level::kPullSpeed, 1e-5);
    CHECK_NEAR(g.launch_velocity(g.pull_to(pouch + Vec2{-30.0f, 0})).length(), 20.0, 1e-4);  // the top speed
}

TEST(pull_for_gives_that_angle_and_speed) {
    const Level g(one_pig());
    const Vec2 v = g.launch_velocity(g.pull_for(0.5f, 13.0f));
    CHECK_NEAR(v.length(), 13.0, 1e-4);
    CHECK_NEAR(std::atan2(v.y, v.x), 0.5, 1e-5);
    CHECK_NEAR(g.launch_velocity(g.pull_for(0.5f, 99.0f)).length(), Level::kMaxPull * Level::kPullSpeed, 1e-4);
    CHECK_NEAR(g.launch_velocity(g.pull_for(0.5f, 0.1f)).length(), Level::kMinPull * Level::kPullSpeed, 1e-4);
}

TEST(a_short_pull_is_not_a_shot) {
    Level g(one_pig());
    CHECK(!g.fire({-0.2f, 0}));
    CHECK(g.shots.empty());
    CHECK(g.fire({-0.3f, 0}));
}

TEST(birds_are_real_bodies_fired_in_the_layouts_order) {
    Level g(one_pig({Ammo::Heavy, Ammo::Ball, Ammo::Blast}));
    const size_t before = g.world.bodies.size();
    const Vec2 pull = g.pull_for(0.3f, 12.0f);
    CHECK(g.next_ammo() == Ammo::Heavy);
    CHECK(g.fire(pull));
    CHECK(g.world.bodies.size() == before + 1);
    const Body& heavy = g.world.bodies.back();
    CHECK(heavy.type == BodyType::Dynamic);
    CHECK(heavy.pos.x == g.launch_point(pull).x && heavy.pos.y == g.launch_point(pull).y);
    CHECK(heavy.vel.x == g.launch_velocity(pull).x && heavy.vel.y == g.launch_velocity(pull).y);
    CHECK_NEAR(heavy.shape.circle.radius, ammo_props(Ammo::Heavy).radius, 1e-6);
    CHECK(g.role(before) == Role::Heavy);
    run(g, 0.5);
    CHECK(g.next_ammo() == Ammo::Ball);
    CHECK(g.fire(kBackwards));
    CHECK(g.role(before + 1) == Role::Ball);
    CHECK(g.world.bodies[before].mass > 5.0f * g.world.bodies[before + 1].mass);
    run(g, 0.5);
    CHECK(g.next_ammo() == Ammo::Blast);
    CHECK(g.shots_left() == 1);
}

TEST(the_sling_reloads_and_runs_out) {
    Level g(one_pig());
    CHECK(g.fire(kBackwards));
    CHECK(!g.fire(kBackwards));  // reloading
    run(g, static_cast<double>(Level::kReloadTime) - 0.05);
    CHECK(!g.can_fire());
    run(g, 0.1);
    CHECK(g.fire(kBackwards));
    run(g, 0.5);
    CHECK(g.fire(kBackwards));
    run(g, 0.5);
    CHECK(g.shots_left() == 0);
    CHECK(!g.can_fire());
    CHECK(!g.fire(kBackwards));
    CHECK(g.shots_used() == 3);
}

// ---- damage ----

TEST(resting_weight_never_hurts_however_heavy) {
    Layout l = empty_level();
    column(l, Material::Stone, 10.0f, kFloorTop, 8, {0.35f, 0.35f});
    column(l, Material::Ice, 14.0f, kFloorTop, 8, {0.35f, 0.35f});
    Level g(l);
    const std::vector<Real> start = g.health;
    run(g, 5.0);
    CHECK(g.health == start);
    CHECK(g.score == 0);
}

TEST(a_gentle_touch_does_no_damage_and_a_hard_hit_does) {
    Layout l = empty_level();
    block(l, Material::Wood, {14.0f, 0.851f}, {0.35f, 0.35f});
    Level g(l);
    const int b = body_of_piece(g, 1);
    const Real full = health_of(g, b);
    CHECK_NEAR(full, material_props(Material::Wood).toughness * 0.49f, 1e-4);
    // A ball rolled into it slower than the damage speed.
    CHECK(g.fire(g.pull_for(0.0f, 4.0f)));
    Body& ball = g.world.bodies[static_cast<size_t>(g.shots[0])];
    ball.pos = {13.0f, 0.751f};
    ball.vel = {0.8f * Level::kDamageSpeed, 0};
    run(g, 1.5);
    CHECK(health_of(g, b) == full);
    // The same ball at 8 m/s. (At full speed a ball breaks a wood block outright.)
    ball.pos = {12.0f, 0.751f};
    ball.vel = {8.0f, 0};
    ball.wake();
    run(g, 0.5);
    CHECK(health_of(g, b) < full * 0.6f);
    CHECK(!g.popped(b));
}

TEST(a_heavy_bird_hits_harder_than_a_ball_at_the_same_speed) {
    Real left[2];
    for (int k = 0; k < 2; ++k) {
        Layout l = empty_level({k == 0 ? Ammo::Ball : Ammo::Heavy});
        block(l, Material::Stone, {14.0f, 0.851f}, {0.35f, 0.35f});
        Level g(l);
        CHECK(g.fire(g.pull_for(0.0f, 10.0f)));
        Body& shot = g.world.bodies[static_cast<size_t>(g.shots[0])];
        shot.pos = {12.0f, 0.86f};
        shot.vel = {10.0f, 0};
        run(g, 0.5);
        left[k] = health_of(g, body_of_piece(g, 1));
    }
    CHECK(left[1] < left[0] - 2.0f);
}

TEST(a_drop_breaks_ice_before_wood_before_stone) {
    // The same block dropped 1.5 m onto the floor: ice shatters, wood cracks, stone shrugs it off.
    Real condition[3];
    bool popped[3];
    const Material m[] = {Material::Ice, Material::Wood, Material::Stone};
    for (int k = 0; k < 3; ++k) {
        Layout l = empty_level();
        block(l, m[k], {14.0f, 0.5f + 1.5f + 0.35f}, {0.35f, 0.35f});
        Level g(l);
        run(g, 2.0);
        const int b = body_of_piece(g, 1);
        popped[k] = g.popped(b);
        condition[k] = popped[k] ? 0 : g.condition(static_cast<size_t>(b));
    }
    CHECK(popped[0]);
    CHECK(!popped[1] && condition[1] < 0.9f);
    CHECK(!popped[2] && condition[2] > condition[1]);
}

TEST(a_pig_pops_from_a_long_fall_but_not_a_short_one) {
    for (const Real height : {0.2f, 3.0f}) {
        Layout l = empty_level();
        pig(l, Pig::Small, 14.0f, kFloorTop + height);
        Level g(l);
        run(g, 2.0);
        CHECK(g.popped(body_of_piece(g, 1)) == (height > 1.0f));
    }
}

TEST(a_popped_piece_is_gone_and_scores) {
    Layout l = empty_level();
    block(l, Material::Wood, {14.0f, 0.851f}, {0.35f, 0.35f});
    block(l, Material::Wood, {14.0f, 0.851f + 0.701f}, {0.35f, 0.35f});
    Level g(l);
    const int lower = body_of_piece(g, 1), upper = lower + 1;
    run(g, 0.5);  // settle the millimetre onto the floor first
    g.health[static_cast<size_t>(lower)] = 0.01f;
    put(g, lower, g.world.bodies[static_cast<size_t>(lower)].pos, {0, -2.0f});  // knocked into the floor
    g.step();
    CHECK(g.popped(lower));
    CHECK(g.role(static_cast<size_t>(lower)) == Role::Hidden);
    CHECK(g.world.bodies[static_cast<size_t>(lower)].mask == 0);
    CHECK(g.score == material_props(Material::Wood).points);
    // And it is logged for the app to draw: where, how many points, what it was.
    CHECK(g.pops.size() == 1);
    CHECK(g.pops[0].points == material_props(Material::Wood).points);
    CHECK(g.pops[0].kind == Kind::Block && g.pops[0].material == Material::Wood);
    CHECK_NEAR(g.pops[0].pos.x, 14.0, 0.05);
    CHECK_NEAR(g.pops[0].size, 0.35, 1e-6);
    CHECK(g.pops[0].time == g.sim_time);
    // What it held up falls through where it was.
    const Real y0 = g.world.bodies[static_cast<size_t>(upper)].pos.y;
    run(g, 1.0);
    CHECK(g.world.bodies[static_cast<size_t>(upper)].pos.y < y0 - 0.6f);
    CHECK(!g.popped(upper));
}

TEST(unbreakable_pieces_take_no_damage) {
    Layout l = empty_level();
    const int b = block(l, Material::Ice, {14.0f, 0.5f + 3.0f + 0.35f}, {0.35f, 0.35f});
    l.pieces[static_cast<size_t>(b)].breakable = false;
    Level g(l);
    run(g, 2.0);
    CHECK(!g.popped(body_of_piece(g, b)));
    CHECK(g.condition(static_cast<size_t>(body_of_piece(g, b))) == 1.0f);
}

// ---- the end of a level ----

TEST(popping_every_pig_wins_with_a_bonus_for_the_birds_left) {
    Layout l = one_pig();
    pig(l, Pig::Big, 10.0f, kFloorTop);  // piece 1
    Level g(l);
    CHECK(g.pigs_left == 2);
    CHECK(g.fire(kBackwards));
    run(g, 0.5);
    for (int p = 0; p < 2; ++p) {
        const int b = body_of_piece(g, p);
        g.health[static_cast<size_t>(b)] = 0.01f;
        put(g, b, g.world.bodies[static_cast<size_t>(b)].pos, {0, -2.0f});
        g.step();
        CHECK(g.popped(b));
    }
    CHECK(g.pigs_left == 0);
    CHECK(g.status == Status::Won);
    CHECK(g.score == pig_props(Pig::Small).points + pig_props(Pig::Big).points + 2 * Level::kBirdBonus);
    CHECK(g.win_time > 0);
    CHECK(!g.can_fire());
}

TEST(stars_count_the_score_against_the_layouts_thresholds) {
    // One small pig (5000) and 10000 for every bird left: 35000, 25000, 15000, 5000 with 0 to 3 wasted.
    const int expected[] = {3, 2, 2, 1};
    for (int wasted = 0; wasted <= 3; ++wasted) {
        Layout l = one_pig();
        l.stars = {15000, 30000};
        Level g(l);
        for (int i = 0; i < wasted; ++i) {
            CHECK(g.fire(kBackwards));
            run(g, 0.5);
        }
        run(g, 0.5);  // settled onto the floor
        CHECK(g.stars() == 0);
        const int b = body_of_piece(g, 0);
        g.health[static_cast<size_t>(b)] = 0.01f;
        put(g, b, g.world.bodies[static_cast<size_t>(b)].pos, {0, -2.0f});
        g.step();
        CHECK(g.status == Status::Won);
        CHECK(g.score == 5000 + (3 - wasted) * Level::kBirdBonus);
        CHECK(g.stars() == expected[wasted]);
    }
}

TEST(running_out_fails_only_once_things_settle_and_a_late_pop_still_wins) {
    Level g(one_pig({Ammo::Ball}));
    CHECK(g.fire(kBackwards));
    run(g, 0.5);
    CHECK(g.status == Status::Playing);  // the bird is still bouncing about
    for (int i = 0; i < 120 * 10 && g.status == Status::Playing; ++i) g.step();
    CHECK(g.status == Status::Failed);
    CHECK(g.since_last_shot() <= static_cast<double>(Level::kSettleTime) + 0.01);
    const int b = body_of_piece(g, 0);
    g.health[static_cast<size_t>(b)] = 0.01f;
    put(g, b, g.world.bodies[static_cast<size_t>(b)].pos, {0, -2.0f});
    g.step();
    CHECK(g.status == Status::Won);
}

TEST(a_pig_stays_put_on_a_nearly_flat_plank_and_rolls_off_a_steep_one) {
    for (const Real degrees : {2.0f, 12.0f}) {
        Layout l = one_pig();
        const Real tilt = degrees * kPi / 180;
        wall(l, {10.0f, 3.0f}, {2.0f, 0.1f}, tilt);
        const Vec2 up{-std::sin(tilt), std::cos(tilt)};
        const int p = pig(l, Pig::Helmet, 0, 0);
        l.pieces[static_cast<size_t>(p)].pos = Vec2{10.0f, 3.0f} + up * (0.1f + pig_props(Pig::Helmet).radius + 0.001f);
        Level g(l);
        const int b = body_of_piece(g, p);
        const Vec2 start = g.world.bodies[static_cast<size_t>(b)].pos;
        run(g, 4.0);
        const Real moved = distance(g.world.bodies[static_cast<size_t>(b)].pos, start);
        CHECK(degrees < 5.0f ? moved < 0.05f : moved > 1.0f);
    }
}

TEST(one_ball_head_on_does_not_pop_a_big_pig) {
    Layout l = one_pig();
    const int p = pig(l, Pig::Big, 12.0f, kFloorTop);
    Level g(l);
    CHECK(g.fire(g.pull_for(0.0f, 4.0f)));
    Body& ball = g.world.bodies[static_cast<size_t>(g.shots[0])];
    ball.pos = {10.5f, 1.0f};
    ball.vel = {Level::kMaxPull * Level::kPullSpeed, 0};  // as hard as the sling throws
    run(g, 0.2);  // the hit, and not the bounce off the far wall
    const int b = body_of_piece(g, p);
    CHECK(!g.popped(b));
    CHECK(g.condition(static_cast<size_t>(b)) < 0.8f);  // but it felt it
}

TEST(a_bird_on_target_pops_the_pig) {
    Level g(one_pig());
    CHECK(g.fire(g.pull_for(0.0f, 4.0f)));
    Body& ball = g.world.bodies[static_cast<size_t>(g.shots[0])];
    ball.pos = {16.5f, 0.81f};
    ball.vel = {14.0f, 0};
    for (int i = 0; i < 120 && g.status == Status::Playing; ++i) g.step();
    CHECK(g.status == Status::Won);
    CHECK(g.stars() >= 1);
}

// ---- blasts ----

TEST(a_blast_goes_off_on_impact_and_pushes_and_damages_whats_near) {
    Layout l = empty_level({Ammo::Blast, Ammo::Ball});
    // Two blocks on the floor, either side of where the blast lands, and one far away.
    block(l, Material::Stone, {9.0f, 0.851f}, {0.35f, 0.35f});
    block(l, Material::Stone, {11.0f, 0.851f}, {0.35f, 0.35f});
    block(l, Material::Stone, {17.5f, 0.851f}, {0.35f, 0.35f});
    Level g(l);
    const size_t left = static_cast<size_t>(body_of_piece(g, 1)), right = left + 1, far = left + 2;
    CHECK(g.fire(g.pull_for(0.0f, 4.0f)));
    const int blast = g.shots[0];
    g.world.bodies[static_cast<size_t>(blast)].pos = {10.0f, 1.2f};
    g.world.bodies[static_cast<size_t>(blast)].vel = {0, -4.0f};
    CHECK(g.blast_live());
    for (int i = 0; i < 60 && g.blast_live(); ++i) g.step();
    CHECK(!g.blast_live());
    CHECK(g.world.bodies[left].vel.x < -1.0f);
    CHECK(g.world.bodies[right].vel.x > 1.0f);
    CHECK(std::fabs(g.world.bodies[left].vel.x + g.world.bodies[right].vel.x) < 0.1f);  // equal and opposite
    CHECK(g.condition(left) < 0.9f && g.condition(right) < 0.9f);
    CHECK(g.world.bodies[far].vel.length() < 1e-3f);  // out of range: untouched
    CHECK(g.condition(far) == 1.0f);
    // The spent shot is out of the game: drawn as nothing, touching nothing, never moving again.
    CHECK(g.role(static_cast<size_t>(blast)) == Role::Hidden);
    const Vec2 parked = g.world.bodies[static_cast<size_t>(blast)].pos;
    run(g, 1.0);
    CHECK(g.world.bodies[static_cast<size_t>(blast)].pos.x == parked.x && g.world.bodies[static_cast<size_t>(blast)].pos.y == parked.y);
    CHECK(g.can_fire());
}

TEST(a_blast_is_stronger_close_in_and_can_be_set_off_in_flight) {
    Layout l = empty_level({Ammo::Blast, Ammo::Ball});
    block(l, Material::Stone, {9.0f, 0.851f}, {0.35f, 0.35f});
    block(l, Material::Stone, {11.6f, 0.851f}, {0.35f, 0.35f});
    Level g(l);
    const size_t near = static_cast<size_t>(body_of_piece(g, 1)), farther = near + 1;
    g.world.gravity = {};  // so the blast can hang in mid air for as long as the test needs
    CHECK(g.fire(g.pull_for(1.2f, 9.0f)));
    Body& blast = g.world.bodies[static_cast<size_t>(g.shots[0])];
    blast.pos = {9.8f, 3.0f};
    blast.vel = {};
    run(g, static_cast<double>(Level::kReloadTime) + 0.1);
    CHECK(g.blast_live() && g.power_ready());
    CHECK(!g.can_fire());  // reloaded, but a blast is still in flight
    CHECK(g.power());
    CHECK(!g.blast_live());
    const Real v_near = g.world.bodies[near].vel.length(), v_far = g.world.bodies[farther].vel.length();
    CHECK(v_near > 2.0f * v_far);
    CHECK(v_far > 0.1f);
    CHECK(g.condition(near) < g.condition(farther));
    CHECK(!g.power());  // nothing left to set off
    CHECK(!g.can_fire());  // an explosion restarts the reload, like a shot
    run(g, static_cast<double>(Level::kReloadTime) + 0.05);
    CHECK(g.can_fire());
}

TEST(the_settle_clock_restarts_when_a_late_blast_goes_off) {
    // The last bird is a blast that hangs in the air for 7.5 s and then goes off next to a disc, which
    // rolls away. Out of birds, the level must wait for the disc to stop: the explosion, not the
    // firing, is what starts the clock.
    Layout l = one_pig({Ammo::Blast});
    const int d = block(l, Material::Stone, {6.0f, 0.801f}, {0.3f, 0.3f});
    l.pieces[static_cast<size_t>(d)].radius = 0.3f;
    Level g(l);
    g.world.gravity = {};
    CHECK(g.fire(g.pull_for(0.0f, 4.0f)));
    Body& blast = g.world.bodies[static_cast<size_t>(g.shots[0])];
    blast.pos = {7.0f, 2.0f};  // the disc rolls off to the left, away from the pig
    blast.vel = {};
    run(g, 7.5);
    CHECK(g.status == Status::Playing);
    g.world.gravity = {0, -9.81f};
    CHECK(g.power());
    run(g, 0.7);  // 8.2 s after firing, but the disc is still rolling
    CHECK(g.status == Status::Playing);
    for (int i = 0; i < 120 * 10 && g.status == Status::Playing; ++i) g.step();
    CHECK(g.status == Status::Failed);
}

// ---- what the app hears and sees ----

TEST(hard_impacts_are_logged_with_their_surfaces_and_gentle_ones_are_not) {
    Layout l = one_pig();
    column(l, Material::Stone, 10.0f, kFloorTop, 5, {0.35f, 0.35f});  // a resting stack: never heard
    Level g(l);
    run(g, 2.0);
    CHECK(g.hits.empty());
    CHECK(g.fire(g.pull_for(0.0f, 4.0f)));
    Body& ball = g.world.bodies[static_cast<size_t>(g.shots[0])];
    ball.pos = {5.0f, 3.0f};
    ball.vel = {0, -6.0f};  // straight down onto the floor
    run(g, 0.4);
    CHECK(!g.hits.empty());
    const Level::Hit& h = g.hits.front();
    CHECK((h.sa == Level::Surface::Bird && h.sb == Level::Surface::Static) || (h.sb == Level::Surface::Bird && h.sa == Level::Surface::Static));
    CHECK(h.impulse > 1.0f);
    CHECK_NEAR(h.pos.x, 5.0, 0.05);
    CHECK_NEAR(h.pos.y, kFloorTop, 0.05);
    g.reset();
    CHECK(g.hits.empty() && g.booms.empty() && g.powers_used.empty());
}

TEST(blasts_and_powers_are_logged) {
    Level g(one_pig({Ammo::Dart, Ammo::Blast}));
    CHECK(g.fire(g.pull_for(0.5f, 10.0f)));
    CHECK(g.power());
    CHECK(g.powers_used.size() == 1 && g.powers_used[0].kind == Ammo::Dart);
    for (int i = 0; i < 600 && !g.can_fire(); ++i) g.step();
    CHECK(g.fire(g.pull_for(0.5f, 10.0f)));
    CHECK(g.power());
    CHECK(g.booms.size() == 1);
    if (!g.booms.empty()) CHECK_NEAR(g.booms[0].radius, Level::kBlastRadius, 1e-6);
}

TEST(a_high_lob_goes_up_and_comes_down_without_hitting_a_ceiling) {
    Level g(one_pig());
    CHECK(g.fire(g.pull_for(1.5707963f, 99.0f)));  // straight up, as hard as the sling throws
    Real top = 0;
    for (int i = 0; i < 120 * 3; ++i) {
        g.step();
        top = std::max(top, g.world.bodies[static_cast<size_t>(g.shots[0])].pos.y);
    }
    CHECK(top > kWorldH + 8.0f);  // far above the view, where the old ceiling was
    for (const Level::Hit& h : g.hits) CHECK(h.pos.y < kWorldH);  // and it touched nothing up there
}

// ---- powers ----

namespace {

// Fires the next bird and puts it at `p` with velocity `v`, in flight and untouched.
int launch(Level& g, Vec2 p, Vec2 v) {
    CHECK(g.fire(g.pull_for(0.5f, 10.0f)));
    const int b = g.shots.back();
    g.world.bodies[static_cast<size_t>(b)].pos = p;
    g.world.bodies[static_cast<size_t>(b)].vel = v;
    return b;
}

// How much health a bird of kind `a` at `speed` takes off one block of material `m` in a head-on hit.
// The block is a big one, so no bird breaks it outright and the numbers can be compared.
Real damage_done(Ammo a, Material m, Real speed) {
    Layout l = one_pig({a});
    const int piece = block(l, m, {14.0f, 1.101f}, {0.6f, 0.6f});
    Level g(l);
    run(g, 0.3);
    launch(g, {12.9f, 1.1f}, {speed, 0});  // just short of the block
    const int b = body_of_piece(g, piece);
    const Real before = g.health[static_cast<size_t>(b)];
    run(g, 0.3);
    CHECK(!g.popped(b));
    return before - g.health[static_cast<size_t>(b)];
}

}  // namespace

TEST(basic_and_heavy_birds_have_no_power) {
    Level g(one_pig({Ammo::Ball, Ammo::Heavy}));
    launch(g, {8.0f, 6.0f}, {5.0f, 0});
    CHECK(!g.power_ready());
    CHECK(!g.power());
    run(g, 0.5);
    CHECK(g.can_fire());
    launch(g, {8.0f, 6.0f}, {5.0f, 0});
    CHECK(!g.power_ready());
}

TEST(a_power_is_for_the_flight_only_and_holds_the_sling_till_then) {
    Level g(one_pig({Ammo::Dart, Ammo::Dart, Ammo::Ball}));
    launch(g, {8.0f, 8.0f}, {3.0f, 0});
    CHECK(g.power_ready());
    run(g, static_cast<double>(Level::kReloadTime) + 0.1);
    CHECK(g.power_ready());
    CHECK(!g.can_fire());  // a click now means the power, not the next bird
    for (int i = 0; i < 300 && g.power_ready(); ++i) g.step();  // it lands
    CHECK(!g.power_ready());
    CHECK(!g.power());  // too late: it has hit something
    CHECK(g.can_fire());
    launch(g, {8.0f, 8.0f}, {3.0f, 0});
    CHECK(g.power());
    CHECK(!g.power());  // once only
    CHECK(!g.power_ready());
}

TEST(a_triple_splits_into_three_at_the_same_speed_fanned_out) {
    Level g(one_pig({Ammo::Split}));
    const size_t before = g.world.bodies.size();
    const Vec2 v{8.0f, 3.0f};
    const int bird = launch(g, {8.0f, 6.0f}, v);
    CHECK(g.power());
    CHECK(g.world.bodies.size() == before + 3);
    CHECK(g.shots_used() == 1);  // still one bird from the sling
    const Real heading = std::atan2(v.y, v.x);
    std::vector<Real> angles;
    for (size_t b = before; b < g.world.bodies.size(); ++b) {
        CHECK(g.shot_of(b) == 0);
        CHECK(g.role(b) == Role::Split);
        CHECK_NEAR(g.world.bodies[b].vel.length(), v.length(), 1e-4);
        angles.push_back(std::atan2(g.world.bodies[b].vel.y, g.world.bodies[b].vel.x) - heading);
        for (size_t c = before; c < b; ++c) {  // and none of them overlap
            phys::Manifold m;
            CHECK(!phys::collide(g.world.bodies[b], g.world.bodies[c], m));
        }
    }
    std::sort(angles.begin(), angles.end());
    CHECK_NEAR(angles[0], -Level::kSplitAngle, 1e-4);
    CHECK_NEAR(angles[1], 0, 1e-4);
    CHECK_NEAR(angles[2], Level::kSplitAngle, 1e-4);
    CHECK(g.world.bodies[static_cast<size_t>(bird)].vel.x == v.x);  // the original carries straight on
}

TEST(a_dart_dashes_along_its_heading) {
    Level g(one_pig({Ammo::Dart}));
    const int bird = launch(g, {8.0f, 6.0f}, {6.0f, 8.0f});
    CHECK(g.power());
    const Vec2 v = g.world.bodies[static_cast<size_t>(bird)].vel;
    CHECK_NEAR(v.length(), Level::kDashSpeed, 1e-4);
    CHECK_NEAR(v.x / v.length(), 0.6, 1e-5);
    CHECK_NEAR(v.y / v.length(), 0.8, 1e-5);
}

TEST(a_dart_at_full_dash_does_not_pass_through_the_thinnest_post) {
    // The engine sweeps fast bodies only against static ones, so this is the bound kDashSpeed was
    // chosen for: a dart cannot cross a 24 cm post in one step. Tried at a spread of heights.
    for (int k = 0; k < 9; ++k) {
        Layout l = one_pig({Ammo::Dart});
        const int p = post(l, Material::Wood, 12.0f, kFloorTop, 3.0f, 0.12f);
        l.pieces[static_cast<size_t>(p)].breakable = false;
        Level g(l);
        // As good as immovable, but still a moving body, so the engine does not sweep against it: a
        // light post would just be knocked over, and that is not what is being tested.
        Body& wall = g.world.bodies[static_cast<size_t>(body_of_piece(g, p))];
        wall.inv_mass = wall.inv_inertia = 1e-6f;
        run(g, 0.2);
        const int bird = launch(g, {10.0f + 0.013f * static_cast<Real>(k), 1.0f + 0.25f * static_cast<Real>(k)}, {8.0f, 0});
        g.world.gravity = {};
        CHECK(g.power());
        run(g, 0.3);
        CHECK(g.world.bodies[static_cast<size_t>(bird)].pos.x < 12.0f);
    }
}

TEST(each_bird_is_hardest_on_its_own_material) {
    // Same speed, same block: a dart is far harder on wood than a basic bird, a triple on ice, a heavy
    // on stone; and a dart or a triple is softer on stone.
    const Real speed = 3.0f;
    CHECK(damage_done(Ammo::Dart, Material::Wood, speed) > 2 * damage_done(Ammo::Ball, Material::Wood, speed));
    CHECK(damage_done(Ammo::Split, Material::Ice, speed) > 1.5f * damage_done(Ammo::Ball, Material::Ice, speed));
    CHECK(damage_done(Ammo::Heavy, Material::Stone, speed) > 2 * damage_done(Ammo::Ball, Material::Stone, speed));
    CHECK(damage_done(Ammo::Dart, Material::Stone, speed) < damage_done(Ammo::Ball, Material::Stone, speed));
    CHECK(damage_done(Ammo::Split, Material::Stone, speed) < damage_done(Ammo::Ball, Material::Stone, speed));
    for (const Ammo a : {Ammo::Ball, Ammo::Heavy, Ammo::Blast, Ammo::Split, Ammo::Dart, Ammo::Dropper})
        for (const Material m : {Material::Wood, Material::Ice, Material::Stone}) CHECK(damage_factor(a, m, true) == 1.0f);  // pigs: as it comes
}

TEST(a_dropper_drops_an_egg_that_explodes_where_it_lands) {
    Layout l = one_pig({Ammo::Dropper});
    const int under = pig(l, Pig::Small, 12.0f, kFloorTop);
    frame(l, Material::Stone, 12.0f, kFloorTop, 1.0f, 0.9f);  // a stone hut round it
    Level g(l);
    run(g, 0.3);
    const size_t before = g.world.bodies.size();
    const int bird = launch(g, {12.0f, 6.0f}, {6.0f, 0});
    CHECK(g.power());
    CHECK(g.world.bodies.size() == before + 2);
    const size_t egg = before + 1;
    CHECK(g.role(egg) == Role::Egg);
    CHECK(g.world.bodies[egg].pos.y < g.world.bodies[static_cast<size_t>(bird)].pos.y);
    CHECK_NEAR(g.world.bodies[egg].vel.y, -Level::kEggSpeed, 1e-5);
    const Vec2 hop = g.world.bodies[static_cast<size_t>(bird)].vel;
    CHECK_NEAR(hop.length(), Level::kHopSpeed, 1e-4);
    CHECK(hop.x > 0 && hop.y > 0);  // up and away, the way it was going
    CHECK(g.blast_live());
    CHECK(!g.can_fire());
    for (int i = 0; i < 120 && g.blast_live(); ++i) g.step();
    CHECK(!g.blast_live());
    CHECK(g.role(egg) == Role::Hidden);
    CHECK(g.popped(body_of_piece(g, under)));  // through the roof
}

// ---- joints ----

TEST(a_motor_hinge_turns_at_its_speed_whatever_hits_it) {
    Layout l = one_pig();
    const int bar = block(l, Material::Wood, {9.0f, 6.0f}, {1.5f, 0.12f});
    l.pieces[static_cast<size_t>(bar)].breakable = false;
    l.hinges.push_back({bar, {9.0f, 6.0f}, true, 1.0f, 3000.0f});
    Level g(l);
    const size_t b = static_cast<size_t>(body_of_piece(g, bar));
    CHECK(g.role(b) == Role::Driven);
    run(g, 2.0);
    CHECK_NEAR(g.world.bodies[b].angle, 2.0, 0.05);
    CHECK_NEAR(g.world.bodies[b].pos.x, 9.0, 0.01);  // pinned at its centre
    CHECK(g.quiet());  // a driven body does not count as movement
}

TEST(a_slider_swings_smoothly_and_carries_what_rests_on_it) {
    Layout l = one_pig();
    const int deck = block(l, Material::Stone, {8.0f, 5.0f}, {1.0f, 0.15f});
    l.pieces[static_cast<size_t>(deck)].breakable = false;
    block(l, Material::Wood, {8.0f, 5.501f}, {0.35f, 0.35f});
    l.sliders.push_back({deck, {1, 0}, 2.0f, 6.0f});
    Level g(l);
    const size_t d = static_cast<size_t>(body_of_piece(g, deck)), box = d + 1;
    for (int i = 0; i < 120 * 6; ++i) {
        g.step();
        const double want = 8.0 + 2.0 * (1 - std::cos(2 * M_PI * g.sim_time / 6.0));  // starts at rest
        CHECK_NEAR(g.world.bodies[d].pos.x, want, 0.03);
        CHECK_NEAR(g.world.bodies[d].pos.y, 5.0, 0.01);
        // The block rides along without sliding: always where it started on the deck.
        CHECK_NEAR(g.world.bodies[box].pos.x - g.world.bodies[d].pos.x, 0.0, 0.03);
    }
    CHECK(g.condition(box) == 1.0f);  // and is not hurt by the ride
}

TEST(a_popped_piece_lets_go_of_its_rope_and_the_other_ropes_still_hold) {
    Layout l = one_pig({Ammo::Blast});
    const int a = block(l, Material::Ice, {8.0f, 6.0f}, {0.3f, 0.3f});
    const int b = block(l, Material::Wood, {11.0f, 6.0f}, {0.3f, 0.3f});
    l.ropes.push_back({a, {8.0f, 9.0f}, {8.0f, 6.0f}});
    l.ropes.push_back({b, {11.0f, 9.0f}, {11.0f, 6.0f}});
    Level g(l);
    const int ba = body_of_piece(g, a), bb = body_of_piece(g, b);
    CHECK(g.world.joints.size() == 2);
    // A blast set off right under the first piece shatters it; the second is out of its reach.
    CHECK(g.fire(g.pull_for(0.0f, 4.0f)));
    g.world.bodies[static_cast<size_t>(g.shots[0])].pos = {8.0f, 5.4f};
    CHECK(g.power());
    CHECK(g.popped(ba));
    CHECK(g.world.joints.size() == 1);
    CHECK(g.world.joints[0].b == bb);
    run(g, 2.0);
    CHECK_NEAR(g.world.bodies[static_cast<size_t>(bb)].pos.y, 6.0, 0.02);  // still hanging
    CHECK(g.condition(static_cast<size_t>(bb)) == 1.0f);
}

TEST(a_slider_keeps_swinging_after_a_piece_jointed_before_it_pops) {
    // The rope's joint comes first, the slider's second; popping the roped piece removes joint 0, so the
    // slider's motor has to follow its joint down to index 0.
    Layout l = one_pig({Ammo::Blast});
    const int roped = block(l, Material::Ice, {6.0f, 6.0f}, {0.3f, 0.3f});
    const int deck = block(l, Material::Stone, {12.0f, 5.0f}, {1.0f, 0.15f});
    l.pieces[static_cast<size_t>(deck)].breakable = false;
    l.ropes.push_back({roped, {6.0f, 9.0f}, {6.0f, 6.0f}});
    l.sliders.push_back({deck, {1, 0}, 2.0f, 6.0f});
    Level g(l);
    CHECK(g.fire(g.pull_for(0.0f, 4.0f)));
    g.world.bodies[static_cast<size_t>(g.shots[0])].pos = {6.0f, 5.4f};
    CHECK(g.power());
    CHECK(g.popped(body_of_piece(g, roped)));
    CHECK(g.world.joints.size() == 1);
    const size_t d = static_cast<size_t>(body_of_piece(g, deck));
    for (int i = 0; i < 120 * 3; ++i) {
        g.step();
        CHECK_NEAR(g.world.bodies[d].pos.x, 12.0 + 2.0 * (1 - std::cos(2 * M_PI * g.sim_time / 6.0)), 0.03);
    }
}

// ---- determinism ----

TEST(a_copy_carries_on_exactly_like_the_original) {
    Layout l = one_pig();
    frame(l, Material::Wood, 12.0f, kFloorTop, 1.2f, 1.2f);
    Level g(l);
    CHECK(g.fire(g.pull_for(0.15f, 15.0f)));
    run(g, 0.3);
    Level copy = g;
    run(g, 2.0);
    run(copy, 2.0);
    CHECK(same_state(g, copy));
}

TEST(reset_restores_the_level_and_counts_attempts) {
    Level g(one_pig());
    const Level fresh(one_pig());
    g.reset();  // nothing fired yet: not an attempt
    CHECK(g.attempt == 1);
    CHECK(g.fire(g.pull_for(0.2f, 14.5f)));
    run(g, 3.0);
    g.reset();
    CHECK(g.attempt == 2);
    CHECK(g.shots.empty() && g.shots_left() == 3 && g.status == Status::Playing && g.sim_time == 0 && g.score == 0);
    CHECK(g.pops.empty());
    CHECK(same_state(g, fresh));
}
