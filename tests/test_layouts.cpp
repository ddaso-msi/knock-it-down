// Every level: it is well formed, it stands up by itself, its recorded solution still wins, and what it
// tells the player holds.
#include "solutions.hpp"
#include "test.hpp"

#include <level.hpp>
#include <levels/levels.hpp>
#include <phys/collision.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <string>

using namespace kid;
using kid::testdata::Solution;

namespace {

Layout layout_named(const std::string& name) {
    for (int i = 0; i < level_count(); ++i)
        if (make_layout(i).name == name) return make_layout(i);
    CHECK(false);
    return {};
}

const Solution* solution_for(const std::string& name) {
    for (const Solution& s : testdata::solutions())
        if (name == s.level) return &s;
    return nullptr;
}

// Plays the recorded shots on their exact steps, then on until the level decides (or 60 s pass).
Level replay(const Solution& s) {
    Level g(layout_named(s.level));
    size_t next = 0;
    for (long step = 0; step < 120L * 60 && g.status == Status::Playing; ++step) {
        for (const testdata::Shot& shot : s.shots)
            if (shot.power == step) CHECK(g.power());
        if (next < s.shots.size() && s.shots[next].step == step) {
            CHECK(g.fire(s.shots[next].pull));
            ++next;
        }
        g.step();
    }
    CHECK(next == s.shots.size());
    return g;
}

// Fires one shot from each point of a grid (every 3 degrees from -20 to 70, every 1 m/s from 6 to 20)
// with `ammo` as the level's first bird, plays until things settle, and counts the shots after which
// `hit` is true.
// With `power_x` set, the bird uses its power as it passes that x.
template <class Hit>
int shots_that(Layout l, Ammo ammo, Hit hit, Real power_x = -1) {
    l.ammo = {ammo, Ammo::Ball};  // a second bird in hand, so the level waits for the first to settle
    const Level start(l);
    int count = 0;
    for (int a = -20; a <= 70; a += 3)
        for (int s = 6; s <= 20; ++s) {
            Level g = start;
            g.fire(g.pull_for(static_cast<Real>(a) * phys::kPi / 180, static_cast<Real>(s)));
            for (int i = 0; i < 120 * 6; ++i) {
                if (power_x > 0 && g.power_ready() && g.world.bodies[static_cast<size_t>(g.shots[0])].pos.x >= power_x) g.power();
                g.step();
                if (g.status != Status::Playing || (g.since_last_shot() > 1.0 && g.quiet() && !g.blast_live())) break;
            }
            count += hit(g);
        }
    return count;
}

int body_of_kind(const Level& g, Pig kind) {
    for (int p : g.pigs)
        if (g.pig_kind(static_cast<size_t>(p)) == kind) return p;
    return -1;
}

}  // namespace

TEST(there_are_fifteen_levels_with_distinct_names) {
    CHECK(level_count() == 15);
    std::set<std::string> names;
    for (int i = 0; i < level_count(); ++i) {
        const Layout l = make_layout(i);
        CHECK(!l.name.empty());
        CHECK(!l.objective.empty());
        names.insert(l.name);
    }
    CHECK(names.size() == static_cast<size_t>(level_count()));
    CHECK(make_layout(-1).name.empty());
    CHECK(make_layout(level_count()).name.empty());
}

TEST(every_level_has_pigs_birds_and_a_par_it_can_afford) {
    for (int i = 0; i < level_count(); ++i) {
        const Layout l = make_layout(i);
        const Level g(l);
        CHECK(!g.pigs.empty());
        CHECK(g.pigs_left == static_cast<int>(g.pigs.size()));
        CHECK(!l.ammo.empty());
        CHECK(l.par >= 1 && l.par <= static_cast<int>(l.ammo.size()));
        CHECK(l.stars.two > 0 && l.stars.two < l.stars.three);
    }
}

TEST(no_level_starts_with_anything_overlapping) {
    for (int i = 0; i < level_count(); ++i) {
        const Level g(make_layout(i));
        const World& w = g.world;
        for (size_t a = 0; a < w.bodies.size(); ++a)
            for (size_t b = a + 1; b < w.bodies.size(); ++b) {
                if (w.bodies[a].type == phys::BodyType::Static && w.bodies[b].type == phys::BodyType::Static) continue;
                phys::Manifold m;
                const bool overlap = phys::collide(w.bodies[a], w.bodies[b], m) && m.depth > 1e-4f;
                if (overlap) std::printf("    level %d: bodies %zu and %zu overlap by %g\n", i + 1, a, b, static_cast<double>(m.depth));
                CHECK(!overlap);
            }
    }
}

TEST(every_level_stands_up_by_itself_unharmed) {
    for (int i = 0; i < level_count(); ++i) {
        Level g(make_layout(i));
        const Level start = g;
        for (int s = 0; s < 120 * 10; ++s) g.step();
        CHECK(g.status == Status::Playing);
        CHECK(g.health == start.health);  // not a scratch on anything
        // Nothing that no motor drives has moved more than 3 cm. (A tower of three frames sinks a couple of
        // centimetres as its six stacked contacts settle; anything that slides or rolls goes much further.)
        if (!g.layout().sliders.empty()) continue;
        for (size_t b = 0; b < g.world.bodies.size(); ++b) {
            if (g.world.bodies[b].type != phys::BodyType::Dynamic || g.role(b) == Role::Driven) continue;
            const Real moved = distance(g.world.bodies[b].pos, start.world.bodies[b].pos);
            if (moved >= 0.03f) std::printf("    level %d: body %zu drifted %g m\n", i + 1, b, static_cast<double>(moved));
            CHECK(moved < 0.03f);
        }
    }
}

TEST(every_level_has_a_recorded_solution_at_par) {
    for (int i = 0; i < level_count(); ++i) {
        const Layout l = make_layout(i);
        const Solution* s = solution_for(l.name);
        CHECK(s != nullptr);
        if (s) CHECK(static_cast<int>(s->shots.size()) == l.par);
    }
}

TEST(every_recorded_solution_still_wins_with_three_stars) {
    for (const Solution& s : testdata::solutions()) {
        const Level g = replay(s);
        if (g.status != Status::Won) std::printf("    %s: not solved (%d pigs left)\n", s.level, g.pigs_left);
        CHECK(g.status == Status::Won);
        CHECK(g.stars() == 3);
    }
}

// ---- the claims a level makes to the player ----

TEST(stone_wall_the_heavy_bird_gets_through_far_more_often_than_a_ball) {
    const Layout l = layout_named("Stone Wall");
    const auto helmet_popped = [](const Level& g) { return g.popped(body_of_kind(g, Pig::Helmet)); };
    const int ball = shots_that(l, Ammo::Ball, helmet_popped), heavy = shots_that(l, Ammo::Heavy, helmet_popped);
    std::printf("    Stone Wall: one ball pops the helmet pig %d times, one heavy bird %d times\n", ball, heavy);
    CHECK(heavy >= 10);
    CHECK(heavy >= 3 * ball);
}

TEST(bunker_nothing_but_a_blast_reaches_the_big_pig) {
    const Layout l = layout_named("Bunker");
    const auto big_hurt = [](const Level& g) {
        const int b = body_of_kind(g, Pig::Big);
        return g.popped(b) || g.condition(static_cast<size_t>(b)) < 1.0f;
    };
    CHECK(shots_that(l, Ammo::Ball, big_hurt) == 0);
    CHECK(shots_that(l, Ammo::Heavy, big_hurt) == 0);
    CHECK(shots_that(l, Ammo::Blast, big_hurt) > 0);
}

TEST(ice_palace_splitting_the_triple_pays) {
    const Layout l = layout_named("Ice Palace");
    const auto three_popped = [](const Level& g) { return static_cast<int>(g.pigs.size()) - g.pigs_left >= 3; };
    const int whole = shots_that(l, Ammo::Split, three_popped), split = shots_that(l, Ammo::Split, three_popped, 9.0f);
    std::printf("    Ice Palace: three pigs or more with one triple: %d shots unsplit, %d split\n", whole, split);
    CHECK(split >= 2 * whole + 5);
}

TEST(timber_yard_a_dashing_dart_tears_through_the_wall) {
    const Layout l = layout_named("Timber Yard");
    const auto wall_broken = [](const Level& g) {
        int broken = 0;
        for (int i = 0; i < 6; ++i) broken += g.popped(g.blocks[static_cast<size_t>(i)]);  // the wall is the first six blocks
        return broken >= 2;
    };
    const int ball = shots_that(l, Ammo::Ball, wall_broken), dart = shots_that(l, Ammo::Dart, wall_broken, 8.0f);
    std::printf("    Timber Yard: two wall blocks broken by %d shots of a basic bird, %d of a dashing dart\n", ball, dart);
    CHECK(dart >= 3 * ball + 5);
}

TEST(rooftops_an_egg_gets_into_a_hut_that_a_bird_cannot) {
    const Layout l = layout_named("Rooftops");
    const auto first_hut = [](const Level& g) { return g.popped(g.pigs[0]); };
    const int thrown = shots_that(l, Ammo::Dropper, first_hut), dropped = shots_that(l, Ammo::Dropper, first_hut, 10.9f);
    std::printf("    Rooftops: first hut's pig popped by %d shots thrown, %d with the egg dropped over it\n", thrown, dropped);
    CHECK(dropped >= 2 * thrown + 5);
}

TEST(windmills_turn_and_sliders_glide_inside_their_levels) {
    // A bar that grinds against something in its own level, or a deck that sticks, would still pass
    // the rule tests in test_level.cpp. The engine keeps angles in [-pi, pi], so turns are added up.
    int checked = 0;
    for (int i = 0; i < level_count(); ++i) {
        const Layout l = make_layout(i);
        if (l.hinges.empty() && l.sliders.empty()) continue;
        Level g(l);
        const size_t first_piece = 4 + l.walls.size();
        std::vector<double> turned(g.world.bodies.size(), 0.0);
        std::vector<Real> x0;
        for (const Body& b : g.world.bodies) x0.push_back(b.pos.x);
        for (int s = 0; s < 120 * 5; ++s) {
            std::vector<Real> before;
            for (const Body& b : g.world.bodies) before.push_back(b.angle);
            g.step();
            for (size_t b = 0; b < before.size(); ++b)
                turned[b] += std::remainder(static_cast<double>(g.world.bodies[b].angle - before[b]), 2 * M_PI);
        }
        for (const Hinge& h : l.hinges) {
            if (!h.motor) continue;
            const size_t b = first_piece + static_cast<size_t>(h.piece);
            CHECK_NEAR(turned[b], h.speed * 5.0, 0.02 * std::fabs(h.speed * 5.0));
            ++checked;
        }
        for (const Slider& sl : l.sliders) {
            const size_t b = first_piece + static_cast<size_t>(sl.piece);
            const double want = static_cast<double>(sl.amplitude) * (1 - std::cos(2 * M_PI * g.sim_time / static_cast<double>(sl.period)));
            CHECK_NEAR(g.world.bodies[b].pos.x - x0[b], want, 0.03);
            ++checked;
        }
    }
    CHECK(checked == 2);
}

TEST(swing_set_every_way_to_the_fort_goes_through_the_wrecking_ball) {
    // Every degree from -20 to 70, every 1 m/s from 6 to 20: no bird touches a pig or a block of the
    // fort before it has touched the wrecking ball.
    const Layout l = layout_named("Swing Set");
    Layout one = l;
    one.ammo = {Ammo::Ball};
    const Level start(one);
    const int ball = 4 + static_cast<int>(l.walls.size()) + static_cast<int>(l.ropes[0].piece);
    int sneaked = 0;
    for (int a = -20; a <= 70; ++a)
        for (int sp = 6; sp <= 20; ++sp) {
            Level g = start;
            g.fire(g.pull_for(static_cast<Real>(a) * phys::kPi / 180, static_cast<Real>(sp)));
            const int bird = g.shots[0];
            bool via_ball = false, sneak = false;
            for (int i = 0; i < 120 * 4 && !via_ball && !sneak; ++i) {
                g.step();
                for (const phys::ContactPair& c : g.world.contacts()) {
                    if (c.manifold.count == 0 || (c.a != bird && c.b != bird)) continue;
                    const int other = c.a == bird ? c.b : c.a;
                    if (other == ball) via_ball = true;
                    else if (g.role(static_cast<size_t>(other)) == Role::Pig || g.role(static_cast<size_t>(other)) == Role::Block) sneak = true;
                }
            }
            if (sneak && !via_ball) {
                std::printf("    Swing Set: %d deg %d m/s reaches the fort directly\n", a, sp);
                ++sneaked;
            }
        }
    CHECK(sneaked == 0);
}
