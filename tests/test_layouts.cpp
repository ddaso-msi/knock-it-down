// Every level: it is well formed, it stands up by itself, and its recorded solution still wins.
#include "solutions.hpp"
#include "test.hpp"

#include <level.hpp>
#include <levels/levels.hpp>
#include <phys/collision.hpp>

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <set>
#include <string>

using namespace kid;
using kid::testdata::Solution;

namespace {

const Solution* solution_for(const std::string& name) {
    for (const Solution& s : testdata::solutions())
        if (name == s.level) return &s;
    return nullptr;
}

// Plays the recorded shots on their exact steps, then on until the level decides (or 30 s pass).
Level replay(const Solution& s) {
    Level g(make_layout(0));
    for (int i = 0; i < level_count(); ++i)
        if (make_layout(i).name == s.level) g = Level(make_layout(i));
    size_t next = 0;
    for (long step = 0; step < 120L * 60 && g.status == Status::Playing; ++step) {
        for (const testdata::Shot& shot : s.shots)
            if (shot.detonate == step) CHECK(g.detonate());
        if (next < s.shots.size() && s.shots[next].step == step) {
            CHECK(g.fire(s.shots[next].aim));
            ++next;
        }
        g.step();
    }
    CHECK(next == s.shots.size());
    return g;
}

}  // namespace

TEST(there_are_twelve_levels_with_distinct_names) {
    CHECK(level_count() == 12);
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

TEST(every_level_has_targets_shots_and_a_par_it_can_afford) {
    for (int i = 0; i < level_count(); ++i) {
        const Layout l = make_layout(i);
        const Level g(l);
        CHECK(!g.targets.empty());
        CHECK(!l.ammo.empty());
        CHECK(l.par >= 1 && l.par <= static_cast<int>(l.ammo.size()));
        CHECK(!l.fall_lines.empty());
        for (const Piece& p : l.pieces)
            if (p.kind != Kind::Loose) CHECK(p.pos.y > p.fall_y);  // nothing starts already down
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

TEST(every_level_stands_up_by_itself) {
    for (int i = 0; i < level_count(); ++i) {
        Level g(make_layout(i));
        const Level start = g;
        for (int s = 0; s < 120 * 10; ++s) g.step();
        CHECK(g.status == Status::Playing);
        CHECK(g.fallen == 0);
        CHECK(g.failure == Failure::None);
        // Pieces that nothing drives stay where they were put (to 2 cm); driven ones keep going, and so
        // does whatever rides on a slider (test_level.cpp checks riding).
        if (!g.layout().sliders.empty()) continue;
        for (size_t b = 0; b < g.world.bodies.size(); ++b) {
            if (g.world.bodies[b].type != phys::BodyType::Dynamic || g.role(b) == Role::Driven) continue;
            const Real moved = distance(g.world.bodies[b].pos, start.world.bodies[b].pos);
            if (moved >= 0.02f) std::printf("    level %d: body %zu drifted %g m\n", i + 1, b, static_cast<double>(moved));
            CHECK(moved < 0.02f);
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
        if (g.status != Status::Won) std::printf("    %s: not solved (%d/%zu down)\n", s.level, g.fallen, g.targets.size());
        CHECK(g.status == Status::Won);
        CHECK(g.stars() == 3);
    }
}

TEST(the_windmill_keeps_turning_and_the_platform_keeps_gliding) {
    for (int i = 0; i < level_count(); ++i) {
        const Layout l = make_layout(i);
        if (l.hinges.empty() && l.sliders.empty()) continue;
        Level g(l);
        const size_t first_piece = g.world.bodies.size() - l.pieces.size();
        std::vector<double> turned(g.world.bodies.size(), 0.0);  // the engine keeps angles in [-pi, pi]: add up steps
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
        }
        for (const Slider& sl : l.sliders) {
            const size_t b = first_piece + static_cast<size_t>(sl.piece);
            const double want = static_cast<double>(sl.amplitude) * (1 - std::cos(2 * M_PI * g.sim_time / static_cast<double>(sl.period)));
            CHECK_NEAR(g.world.bodies[b].pos.x - x0[b], want, 0.03);
        }
    }
}

// ---- the claims a level makes to the player ----

namespace {

Layout layout_named(const std::string& name) {
    for (int i = 0; i < level_count(); ++i)
        if (make_layout(i).name == name) return make_layout(i);
    CHECK(false);
    return {};
}

// The most targets any single shot on a coarse grid (every 3 degrees from -20 to 70, every 1 m/s from
// 6 to 20) gets down, played until things settle. `ammo` replaces the level's first shot.
int best_single_shot(Layout l, Ammo ammo) {
    l.ammo = {ammo, Ammo::Ball};  // a second shot in hand, so the level waits for the first to settle
    const Level start(l);
    int best = 0;
    for (int a = -20; a <= 70; a += 3)
        for (int s = 6; s <= 20; ++s) {
            Level g = start;
            g.fire(g.aim_for(static_cast<Real>(a) * phys::kPi / 180, static_cast<Real>(s)));
            for (int i = 0; i < 120 * 6; ++i) {
                g.step();
                if (g.status != Status::Playing || (g.since_last_shot() > 1.0 && g.quiet())) break;
            }
            best = std::max(best, g.fallen);
        }
    return best;
}

// How many shots (every degree from -20 to 70, every 1 m/s from 6 to 20) touch a target before they have touched any loose piece: shots that
// get past whatever is meant to stand in their way.
int shots_that_sneak_past_the_loose_pieces(const Layout& l) {
    Layout one = l;
    one.ammo = {Ammo::Ball};
    const Level start(one);
    std::vector<int> loose;
    for (size_t b = 0; b < start.world.bodies.size(); ++b)
        if (start.role(b) == Role::Loose || start.role(b) == Role::Driven) loose.push_back(static_cast<int>(b));
    const auto is_in = [](const std::vector<int>& v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); };
    int sneaked = 0;
    for (int a = -20; a <= 70; ++a)  // every degree: a gap is narrow
        for (int s = 6; s <= 20; ++s) {
            Level g = start;
            g.fire(g.aim_for(static_cast<Real>(a) * phys::kPi / 180, static_cast<Real>(s)));
            const int shot = g.shots[0];
            bool blocked = false, sneak = false;
            for (int i = 0; i < 120 * 4 && !blocked && !sneak; ++i) {
                g.step();
                for (const phys::ContactPair& c : g.world.contacts()) {
                    if (c.manifold.count == 0 || (c.a != shot && c.b != shot)) continue;
                    const int other = c.a == shot ? c.b : c.a;
                    if (is_in(loose, other)) blocked = true;
                    else if (is_in(g.targets, other)) sneak = true;
                }
            }
            if (sneak && !blocked) {
                std::printf("    %s: %d deg %d m/s reaches a target directly\n", l.name.c_str(), a, s);
                ++sneaked;
            }
        }
    return sneaked;
}

}  // namespace

TEST(domino_run_every_way_to_the_blocks_goes_through_the_dominoes) {
    CHECK(shots_that_sneak_past_the_loose_pieces(layout_named("Domino Run")) == 0);
}

TEST(blast_wall_only_a_blast_moves_the_blocks) {
    const Layout l = layout_named("Blast Wall");
    CHECK(best_single_shot(l, Ammo::Ball) == 0);
    CHECK(best_single_shot(l, Ammo::Heavy) == 0);
    CHECK(best_single_shot(l, Ammo::Blast) == 3);
}

TEST(wrecking_ball_every_way_to_the_stack_goes_through_the_wrecking_ball) {
    const Layout l = layout_named("Wrecking Ball");
    CHECK(shots_that_sneak_past_the_loose_pieces(l) == 0);
    CHECK(best_single_shot(l, Ammo::Ball) >= 3);  // and through it, one shot does real damage
}

TEST(heavy_going_one_ball_is_not_enough) {
    const Layout l = layout_named("Heavy Going");
    CHECK(best_single_shot(l, Ammo::Ball) < 3);
    CHECK(best_single_shot(l, Ammo::Heavy) == 3);
}
