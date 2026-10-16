// The rules, on small hand-built layouts: shots, fall lines, keeps, the end of a level, blasts, motors.
#include "test.hpp"

#include <level.hpp>

#include <cmath>

using namespace kid;
using namespace phys;

namespace {

constexpr Vec2 kBackwards{0.6f, 2.6f};  // into the wall behind the launcher: a shot that does nothing

void run(Level& g, double seconds) {
    const long steps = std::lround(seconds / static_cast<double>(kTimeStep));
    for (long i = 0; i < steps; ++i) g.step();
}

// One platform at x = 14 with a single wooden block on it.
Layout one_block(std::vector<Ammo> ammo = {Ammo::Ball, Ammo::Ball, Ammo::Ball}) {
    Layout l;
    l.name = "test";
    l.objective = "test";
    l.ammo = std::move(ammo);
    l.par = 1;
    const Real line = platform(l, 14.0f, 3.0f, 0.8f);
    piece(l, Kind::Target, Material::Wood, {14.0f, 3.351f}, {0.35f, 0.35f}, line);
    return l;
}

int body_of_piece(const Level& g, int piece) {  // pieces come after the scenery and before any shot
    return static_cast<int>(g.world.bodies.size() - g.layout().pieces.size() - g.shots.size()) + piece;
}

// Moves a body by hand (as if something had knocked it there) and wakes it.
void put(Level& g, int body, Vec2 p) {
    g.world.bodies[static_cast<size_t>(body)].pos = p;
    g.world.bodies[static_cast<size_t>(body)].vel = {};
    g.world.wake(body);
}

bool same_state(const Level& a, const Level& b) {
    if (a.world.bodies.size() != b.world.bodies.size()) return false;
    for (size_t i = 0; i < a.world.bodies.size(); ++i) {
        const Body &p = a.world.bodies[i], &q = b.world.bodies[i];
        if (p.pos.x != q.pos.x || p.pos.y != q.pos.y || p.angle != q.angle || p.vel.x != q.vel.x || p.vel.y != q.vel.y || p.w != q.w) return false;
    }
    return a.status == b.status && a.fallen == b.fallen && a.sim_time == b.sim_time && a.shots == b.shots;
}

}  // namespace

TEST(launch_speed_is_clamped_and_points_at_the_aim) {
    const Level g(one_block());
    const Vec2 m = g.layout().muzzle;
    CHECK_NEAR(g.launch_velocity(m + Vec2{0.1f, 0}).length(), Level::kMinSpeed, 1e-4);
    const Vec2 far = g.launch_velocity(m + Vec2{30.0f, 40.0f});
    CHECK_NEAR(far.length(), Level::kMaxSpeed, 1e-3);
    CHECK_NEAR(far.y / far.x, 40.0 / 30.0, 1e-4);
    const Vec2 mid = g.launch_velocity(m + Vec2{0, 5.0f});
    CHECK_NEAR(mid.x, 0, 1e-5);
    CHECK_NEAR(mid.y, 5.0f * Level::kSpeedPerMeter, 1e-4);
    CHECK(std::isfinite(g.launch_velocity(m).x));  // aiming exactly at the muzzle
}

TEST(aim_for_gives_that_angle_and_speed) {
    const Level g(one_block());
    const Vec2 v = g.launch_velocity(g.aim_for(0.5f, 13.0f));
    CHECK_NEAR(v.length(), 13.0, 1e-4);
    CHECK_NEAR(std::atan2(v.y, v.x), 0.5, 1e-5);
    CHECK_NEAR(g.launch_velocity(g.aim_for(0.5f, 99.0f)).length(), Level::kMaxSpeed, 1e-4);
    // Out-of-range speeds are clamped in the aim point itself, so it stays near the launcher (on screen).
    CHECK_NEAR(distance(g.aim_for(0.5f, 99.0f), g.layout().muzzle), Level::kMaxSpeed / Level::kSpeedPerMeter, 1e-4);
    CHECK_NEAR(distance(g.aim_for(0.5f, 1.0f), g.layout().muzzle), Level::kMinSpeed / Level::kSpeedPerMeter, 1e-4);
}

TEST(shots_are_real_bodies_fired_in_the_layouts_order) {
    Level g(one_block({Ammo::Heavy, Ammo::Ball, Ammo::Blast}));
    const size_t before = g.world.bodies.size();
    const Vec2 aim = g.aim_for(0.3f, 12.0f);
    CHECK(g.next_ammo() == Ammo::Heavy);
    CHECK(g.fire(aim));
    CHECK(g.world.bodies.size() == before + 1);
    const Body& heavy = g.world.bodies.back();
    CHECK(heavy.type == BodyType::Dynamic);
    CHECK(heavy.pos.x == g.layout().muzzle.x && heavy.pos.y == g.layout().muzzle.y);
    CHECK(heavy.vel.x == g.launch_velocity(aim).x && heavy.vel.y == g.launch_velocity(aim).y);
    CHECK_NEAR(heavy.shape.circle.radius, ammo_props(Ammo::Heavy).radius, 1e-6);
    CHECK(g.role(before) == Role::Heavy);
    run(g, 0.5);
    CHECK(g.next_ammo() == Ammo::Ball);
    CHECK(g.fire(kBackwards));
    CHECK(g.role(before + 1) == Role::Ball);
    // A heavy shot outweighs a ball five times over.
    CHECK(g.world.bodies[before].mass > 5.0f * g.world.bodies[before + 1].mass);
    run(g, 0.5);
    CHECK(g.next_ammo() == Ammo::Blast);
    CHECK(g.shots_left() == 1);
}

TEST(the_launcher_reloads_and_runs_out) {
    Level g(one_block());
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

TEST(a_target_is_down_only_below_its_own_fall_line) {
    Layout l;
    l.ammo = {Ammo::Ball};
    const Real low = platform(l, 8.0f, 2.0f, 0.6f), high = platform(l, 14.0f, 6.0f, 0.6f);
    piece(l, Kind::Target, Material::Wood, {8.0f, 2.351f}, {0.35f, 0.35f}, low);
    piece(l, Kind::Target, Material::Wood, {14.0f, 6.351f}, {0.35f, 0.35f}, high);
    Level g(l);
    const int a = body_of_piece(g, 0), b = body_of_piece(g, 1);
    put(g, b, {17.0f, 3.0f});  // off the high platform, but still above the LOW platform's line
    g.step();
    CHECK(g.fallen == 1);
    CHECK(g.role(static_cast<size_t>(b)) == Role::Done);
    CHECK(g.role(static_cast<size_t>(a)) == Role::Target);
    CHECK(g.status == Status::Playing);
    put(g, a, {11.0f, 0.9f});
    g.step();
    CHECK(g.fallen == 2);
    CHECK(g.status == Status::Won);  // no keeps: a win counts at once
}

TEST(a_thin_piece_lying_on_its_platform_is_not_down) {
    // The fall line sits under the platform's top: whatever rests on the platform, however flat, is up.
    Layout l = one_block();
    piece(l, Kind::Target, Material::Wood, {14.3f, 3.051f}, {0.2f, 0.05f}, l.fall_lines[0].y);
    l.pieces[0].pos.x = 13.8f;
    Level g(l);
    run(g, 1.0);
    CHECK(g.fallen == 0);
    CHECK(g.layout().fall_lines[0].y < 3.0f);
    CHECK(g.layout().fall_lines[0].y > 2.8f);
}

TEST(a_good_shot_wins_and_no_more_shots_can_be_fired) {
    Level g(one_block());
    CHECK(g.fire(g.aim_for(0.35f, 14.5f)));  // flat and hard into the block
    for (int i = 0; i < 120 * 6 && g.status == Status::Playing; ++i) g.step();
    CHECK(g.status == Status::Won);
    CHECK(g.stars() == 3);
    CHECK(g.win_time > 0);
    CHECK(!g.can_fire());
    CHECK(!g.fire(kBackwards));
}

TEST(stars_count_shots_against_par) {
    // Par is 1 here: winning with 0 or 1 shots used is three stars, 2 is two, 3 is one.
    const int expected[] = {3, 3, 2, 1};
    for (int wasted = 0; wasted <= 3; ++wasted) {
        Level g(one_block());
        for (int i = 0; i < wasted; ++i) {
            CHECK(g.fire(kBackwards));
            run(g, 0.5);
        }
        CHECK(g.stars() == 0);
        put(g, body_of_piece(g, 0), {17.0f, 0.9f});
        g.step();
        CHECK(g.status == Status::Won);
        CHECK(g.shots_used() == wasted);
        CHECK(g.stars() == expected[wasted]);
    }
}

TEST(running_out_fails_only_once_things_settle_and_a_late_fall_still_wins) {
    Level g(one_block({Ammo::Ball}));
    CHECK(g.fire(kBackwards));
    run(g, 0.5);
    CHECK(g.status == Status::Playing);  // the ball is still rolling about
    for (int i = 0; i < 120 * 10 && g.status == Status::Playing; ++i) g.step();
    CHECK(g.status == Status::Failed);
    CHECK(g.failure == Failure::OutOfShots);
    CHECK(g.since_last_shot() <= static_cast<double>(Level::kSettleTime) + 0.01);
    put(g, body_of_piece(g, 0), {17.0f, 0.9f});  // the block goes after all
    g.step();
    CHECK(g.status == Status::Won);
    CHECK(g.failure == Failure::None);
}

TEST(losing_a_keep_fails_at_once_and_for_good) {
    Layout l = one_block();
    const Real line = platform(l, 8.0f, 2.0f, 0.6f);
    piece(l, Kind::Keep, Material::Wood, {8.0f, 2.351f}, {0.35f, 0.35f}, line);
    Level g(l);
    const int target = body_of_piece(g, 0), keep = body_of_piece(g, 1);
    CHECK(g.role(static_cast<size_t>(keep)) == Role::Keep);
    put(g, keep, {6.0f, 0.9f});
    g.step();
    CHECK(g.status == Status::Failed);
    CHECK(g.failure == Failure::KeepLost);
    CHECK(g.role(static_cast<size_t>(keep)) == Role::Lost);
    CHECK(!g.can_fire());
    put(g, target, {17.0f, 0.9f});  // clearing the targets afterwards does not help
    run(g, 2.0);
    CHECK(g.status == Status::Failed);
}

TEST(with_keeps_a_win_has_to_hold_for_a_second) {
    Layout l = one_block();
    const Real line = platform(l, 8.0f, 2.0f, 0.6f);
    piece(l, Kind::Keep, Material::Wood, {8.0f, 2.351f}, {0.35f, 0.35f}, line);
    Level g(l);
    put(g, body_of_piece(g, 0), {17.0f, 0.9f});
    run(g, static_cast<double>(Level::kWinHold) - 0.1);
    CHECK(g.status == Status::Playing);
    put(g, body_of_piece(g, 1), {6.0f, 0.9f});  // the keep falls while the win is on hold
    g.step();
    CHECK(g.status == Status::Failed);

    Level h(l);
    put(h, body_of_piece(h, 0), {17.0f, 0.9f});
    run(h, static_cast<double>(Level::kWinHold) + 0.05);
    CHECK(h.status == Status::Won);
}

TEST(a_blast_goes_off_on_impact_and_pushes_things_away) {
    Layout l = one_block({Ammo::Blast, Ammo::Ball});
    // Two blocks on the floor, either side of where the blast lands, and one far away.
    piece(l, Kind::Loose, Material::Wood, {9.0f, 0.851f}, {0.35f, 0.35f}, 0);
    piece(l, Kind::Loose, Material::Wood, {11.0f, 0.851f}, {0.35f, 0.35f}, 0);
    piece(l, Kind::Loose, Material::Wood, {17.5f, 0.851f}, {0.35f, 0.35f}, 0);
    Level g(l);
    const size_t left = static_cast<size_t>(body_of_piece(g, 1)), right = left + 1, far = left + 2;
    // Straight down onto the floor between the two near blocks: from just above, at the slowest speed.
    CHECK(g.fire(g.aim_for(0.0f, 4.0f)));
    const int blast = g.shots[0];
    g.world.bodies[static_cast<size_t>(blast)].pos = {10.0f, 1.2f};
    g.world.bodies[static_cast<size_t>(blast)].vel = {0, -4.0f};
    CHECK(g.live_blast == blast);
    CHECK(!g.can_fire());  // not while a blast is in flight
    for (int i = 0; i < 60 && g.live_blast >= 0; ++i) g.step();
    CHECK(g.live_blast < 0);
    CHECK(g.world.bodies[left].vel.x < -1.0f);
    CHECK(g.world.bodies[right].vel.x > 1.0f);
    CHECK(std::fabs(g.world.bodies[left].vel.x + g.world.bodies[right].vel.x) < 0.1f);  // equal and opposite
    CHECK(g.world.bodies[far].vel.length() < 1e-3f);                                    // out of range
    // The spent shot is out of the game: drawn as nothing, touching nothing, never moving again.
    CHECK(g.role(static_cast<size_t>(blast)) == Role::Hidden);
    CHECK(g.world.bodies[static_cast<size_t>(blast)].mask == 0);
    const Vec2 parked = g.world.bodies[static_cast<size_t>(blast)].pos;
    run(g, 1.0);
    CHECK(g.world.bodies[static_cast<size_t>(blast)].pos.x == parked.x && g.world.bodies[static_cast<size_t>(blast)].pos.y == parked.y);
    CHECK(g.can_fire());
}

TEST(a_blast_is_stronger_close_in) {
    Layout l = one_block({Ammo::Blast, Ammo::Ball});
    piece(l, Kind::Loose, Material::Wood, {9.0f, 0.851f}, {0.35f, 0.35f}, 0);
    piece(l, Kind::Loose, Material::Wood, {11.6f, 0.851f}, {0.35f, 0.35f}, 0);
    Level g(l);
    const size_t near = static_cast<size_t>(body_of_piece(g, 1)), farther = near + 1;
    g.world.gravity = {};  // so the blast can hang in mid air for as long as the test needs
    CHECK(g.fire(g.aim_for(1.2f, 9.0f)));
    Body& blast = g.world.bodies[static_cast<size_t>(g.shots[0])];
    blast.pos = {9.8f, 3.0f};
    blast.vel = {};
    run(g, static_cast<double>(Level::kReloadTime) + 0.1);
    CHECK(g.live_blast == g.shots[0]);
    CHECK(!g.can_fire());    // reloaded, but a blast is still in flight
    CHECK(g.press({0, 0}));  // so a click sets it off rather than firing
    CHECK(g.live_blast < 0);
    CHECK(g.shots_used() == 1);
    const Real v_near = g.world.bodies[near].vel.length(), v_far = g.world.bodies[farther].vel.length();
    CHECK(v_near > 2.0f * v_far);
    CHECK(v_far > 0.1f);
    CHECK(g.world.bodies[near].vel.x < 0 && g.world.bodies[farther].vel.x > 0);
    CHECK(!g.detonate());  // nothing left to set off
    CHECK(!g.can_fire());  // an explosion restarts the reload, like a shot
    run(g, static_cast<double>(Level::kReloadTime) + 0.05);
    CHECK(g.can_fire());
}

TEST(the_settle_clock_restarts_when_a_late_blast_goes_off) {
    // The last shot is a blast that hangs in the air for 7.5 s and then goes off next to a disc, which
    // rolls away. Out of shots, the level must wait for the disc to stop: the explosion, not the firing,
    // is what starts the clock.
    Layout l = one_block({Ammo::Blast});
    disc(l, Kind::Loose, Material::Wood, {6.0f, 0.801f}, 0.3f, 0);
    Level g(l);
    g.world.gravity = {};
    CHECK(g.fire(g.aim_for(0.0f, 4.0f)));
    Body& blast = g.world.bodies[static_cast<size_t>(g.shots[0])];
    blast.pos = {5.0f, 2.0f};
    blast.vel = {};
    run(g, 7.5);
    CHECK(g.status == Status::Playing);
    g.world.gravity = {0, -9.81f};
    CHECK(g.detonate());
    run(g, 0.7);  // 8.2 s after firing, but the disc is still rolling
    CHECK(g.status == Status::Playing);
    for (int i = 0; i < 120 * 10 && g.status == Status::Playing; ++i) g.step();
    CHECK(g.status == Status::Failed);
}

TEST(a_motor_hinge_turns_at_its_speed_whatever_hits_it) {
    Layout l = one_block();
    const int bar = piece(l, Kind::Loose, Material::Wood, {9.0f, 6.0f}, {1.5f, 0.12f}, 0);
    l.hinges.push_back({bar, {9.0f, 6.0f}, true, 1.0f, 3000.0f});
    Level g(l);
    const size_t b = static_cast<size_t>(body_of_piece(g, bar));
    CHECK(g.role(b) == Role::Driven);
    run(g, 2.0);
    CHECK_NEAR(g.world.bodies[b].angle, 2.0, 0.05);
    CHECK_NEAR(g.world.bodies[b].pos.x, 9.0, 0.01);  // pinned at its centre
    CHECK(g.quiet());  // a driven body does not count as movement
    CHECK(g.world.bodies[b].awake);
}

TEST(a_slider_swings_smoothly_and_carries_what_rests_on_it) {
    Layout l;
    l.ammo = {Ammo::Ball};
    const int deck = piece(l, Kind::Loose, Material::Stone, {10.0f, 5.0f}, {1.0f, 0.15f}, 0);
    piece(l, Kind::Target, Material::Wood, {10.0f, 5.501f}, {0.35f, 0.35f}, 4.9f);
    l.sliders.push_back({deck, {1, 0}, 2.0f, 6.0f});
    Level g(l);
    const size_t d = static_cast<size_t>(body_of_piece(g, deck)), block = d + 1;
    for (int i = 0; i < 120 * 6; ++i) {
        g.step();
        const double want = 10.0 + 2.0 * (1 - std::cos(2 * M_PI * g.sim_time / 6.0));  // starts at rest
        CHECK_NEAR(g.world.bodies[d].pos.x, want, 0.03);
        CHECK_NEAR(g.world.bodies[d].pos.y, 5.0, 0.01);
        CHECK_NEAR(g.world.bodies[d].angle, 0, 0.01);
        // The block rides along without sliding: always where it started on the deck.
        CHECK_NEAR(g.world.bodies[block].pos.x - g.world.bodies[d].pos.x, 0.0, 0.03);
    }
    CHECK(g.status == Status::Playing);
}

TEST(a_copy_carries_on_exactly_like_the_original) {
    Level g(one_block());
    CHECK(g.fire(g.aim_for(0.4f, 15.0f)));
    run(g, 0.3);
    Level copy = g;
    run(g, 2.0);
    run(copy, 2.0);
    CHECK(same_state(g, copy));
}

TEST(reset_restores_the_level_and_counts_attempts) {
    Level g(one_block());
    const Level fresh(one_block());
    g.reset();  // nothing fired yet: not an attempt
    CHECK(g.attempt == 1);
    CHECK(g.fire(g.aim_for(0.35f, 14.5f)));
    run(g, 3.0);
    g.reset();
    CHECK(g.attempt == 2);
    CHECK(g.shots.empty() && g.shots_left() == 3 && g.status == Status::Playing && g.sim_time == 0);
    CHECK(same_state(g, fresh));
    // And the same shots replay the same way.
    Level h(one_block());
    CHECK(g.fire(g.aim_for(0.35f, 14.5f)));
    CHECK(h.fire(h.aim_for(0.35f, 14.5f)));
    run(g, 3.0);
    run(h, 3.0);
    CHECK(same_state(g, h));
}
