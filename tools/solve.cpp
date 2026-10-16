// kid_solve: searches a level for winning shots. This is how every level is checked to be solvable, how
// its par is found, and where the recorded solutions in game/levels/solutions.hpp come from.
//
//   kid_solve --level N [--depth D] [--beam K] [--angles A] [--speeds S] [--waits 0,0.5,1] [--detonate 0,0.2]
//             [--ammo bbh]   (try other ammo: b ball, h heavy, x blast)
//
// A shot is a launch angle and speed. Every shot on an angle x speed grid is tried from the level's
// start; the best K results (most targets down) are kept, and the grid is tried again from each of
// them, up to D shots. Shot n+1 is fired once the scene is quiet again (or after 4 s), plus each of the
// extra --waits (seconds), which matter in levels with moving parts. A blast either goes off when it
// hits something, or `--detonate` seconds after firing (each value tried; 0 means on impact only).
//
// For every depth it prints how many grid shots win from the best state, and how many of those are
// ROBUST: the four neighbouring grid shots win too, so a player who is a little off still wins. The
// printed solution is the most robust winner, as exact fire steps, ready to paste into solutions.hpp.
#include <level.hpp>
#include <levels/levels.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace kid;

namespace {

struct Shot {
    long step;   // the fixed step at which it is fired (counted from the level's start)
    Vec2 aim;
    long detonate = -1;  // for a blast: the step at which it is set off, or -1 for on impact
};

struct Node {
    Level level;
    long steps = 0;
    std::vector<Shot> shots;
    explicit Node(Level l) : level(std::move(l)) {}
};

struct Options {
    int level = 1, depth = 9, beam = 6, angles = 91, speeds = 29;
    std::vector<Ammo> ammo;  // replaces the level's own, to ask "could it be done with these?"
    std::vector<double> waits{0};
    std::vector<double> detonate{0};
    Real angle_lo = -20, angle_hi = 70, speed_lo = 6, speed_hi = 20;
};

std::vector<double> parse_list(const char* s) {
    std::vector<double> out;
    for (const char* p = s; *p;) {
        out.push_back(std::atof(p));
        while (*p && *p != ',') ++p;
        if (*p == ',') ++p;
    }
    return out;
}

void advance(Node& n) {
    n.level.step();
    ++n.steps;
}

// Waits until the next shot may be fired: the launcher is ready and the scene is quiet (or 4 s passed).
bool wait_for_turn(Node& n, double extra) {
    while (n.level.status == Status::Playing && (!n.level.can_fire() || !(n.level.quiet() || n.level.since_last_shot() >= 4.0)))
        advance(n);
    const long extra_steps = std::lround(extra / static_cast<double>(kTimeStep));
    for (long i = 0; i < extra_steps && n.level.status == Status::Playing; ++i) advance(n);
    return n.level.can_fire();
}

// Plays the shot and then on until the scene settles or the level decides.
void play(Node& n, Vec2 aim, double detonate_after) {
    Shot s{n.steps, aim};
    n.level.fire(aim);
    if (detonate_after > 0 && n.level.live_blast >= 0) s.detonate = n.steps + std::lround(detonate_after / static_cast<double>(kTimeStep));
    n.shots.push_back(s);
    const bool last = n.level.shots_left() == 0;
    for (int i = 0; i < 120 * 12 && n.level.status == Status::Playing; ++i) {
        if (s.detonate == n.steps) n.level.detonate();
        advance(n);
        const bool all_down = n.level.fallen == static_cast<int>(n.level.targets.size());  // a win may still be on hold
        if (!last && !all_down && n.level.live_blast < 0 && n.level.since_last_shot() > 0.5 && n.level.quiet()) break;
    }
    if (n.level.live_blast >= 0) n.shots.back().detonate = -1;  // never went off in time: treat as on impact
}

double score(const Level& l) {
    if (l.status == Status::Won) return 1e9;
    if (l.failure == Failure::KeepLost) return -1;
    // Targets down first; then how far the rest have been moved, so a dented stack beats an untouched one.
    double moved = 0;
    for (int t : l.targets) {
        const Body& b = l.world.bodies[static_cast<size_t>(t)];
        moved += std::min<double>(2.0, std::fabs(b.angle)) + static_cast<double>(std::min<Real>(3.0f, std::fabs(b.vel.x)));
    }
    return l.fallen * 100.0 + moved;
}

// Replays shots on their exact steps (as the tests do), then on until the level decides.
Level replay(const Layout& layout, const std::vector<Shot>& shots) {
    Level g(layout);
    size_t next = 0;
    for (long step = 0; step < 120L * 60 && g.status == Status::Playing; ++step) {
        for (const Shot& s : shots)
            if (s.detonate == step) g.detonate();
        if (next < shots.size() && shots[next].step == step) g.fire(shots[next++].aim);
        g.step();
    }
    return g;
}

// How forgiving a solution is: the same shots with each aim point moved up to 2 cm in a random direction.
int jitter(const Layout& layout, std::vector<Shot> shots) {
    std::uint32_t seed = 12345;
    const auto rnd = [&] {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<Real>(seed >> 8) / static_cast<Real>(1 << 24);
    };
    const std::vector<Shot> base = shots;
    int wins = 0;
    for (int trial = 0; trial < 100; ++trial) {
        for (size_t i = 0; i < shots.size(); ++i) {
            const Real a = rnd() * 2 * phys::kPi, r = 0.02f * std::sqrt(rnd());
            shots[i].aim = base[i].aim + Vec2{std::cos(a), std::sin(a)} * r;
        }
        wins += replay(layout, shots).status == Status::Won;
    }
    return wins;
}

struct Trial {
    int angle, speed, wait, det;
    std::optional<Node> result;
    double score = 0;
};

Options parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i + 1 < argc; i += 2) {
        const char* a = argv[i];
        const char* v = argv[i + 1];
        if (!std::strcmp(a, "--level")) o.level = std::atoi(v);
        else if (!std::strcmp(a, "--depth")) o.depth = std::atoi(v);
        else if (!std::strcmp(a, "--beam")) o.beam = std::atoi(v);
        else if (!std::strcmp(a, "--angles")) o.angles = std::atoi(v);
        else if (!std::strcmp(a, "--speeds")) o.speeds = std::atoi(v);
        else if (!std::strcmp(a, "--waits")) o.waits = parse_list(v);
        else if (!std::strcmp(a, "--detonate")) o.detonate = parse_list(v);
        else if (!std::strcmp(a, "--ammo")) {
            for (const char* p = v; *p; ++p)
                if (*p == 'b') o.ammo.push_back(Ammo::Ball);
                else if (*p == 'h') o.ammo.push_back(Ammo::Heavy);
                else if (*p == 'x') o.ammo.push_back(Ammo::Blast);
        }
        else if (!std::strcmp(a, "--angle-range")) { o.angle_lo = static_cast<Real>(std::atof(v)); o.angle_hi = static_cast<Real>(std::atof(argv[i + 2])); ++i; }
        else std::fprintf(stderr, "ignoring %s\n", a);
    }
    return o;
}

}  // namespace

int main(int argc, char** argv) {
    const Options o = parse(argc, argv);
    if (o.level < 1 || o.level > level_count()) {
        std::fprintf(stderr, "level must be 1..%d\n", level_count());
        return 1;
    }
    Layout layout = make_layout(o.level - 1);
    if (!o.ammo.empty()) layout.ammo = o.ammo;
    std::printf("Level %d: %s   (%zu shots, par %d)\n", o.level, layout.name.c_str(), layout.ammo.size(), layout.par);

    // Unshot, does it stay up? A level that falls apart by itself is broken.
    {
        Node idle(Level{layout});
        for (int i = 0; i < 120 * 10; ++i) advance(idle);
        std::printf("  untouched for 10 s: %d/%zu targets down, %s\n", idle.level.fallen, idle.level.targets.size(),
                    idle.level.failure == Failure::KeepLost ? "a KEEP FELL" : "keeps fine");
    }

    const auto angle_of = [&](int i) { return (o.angle_lo + (o.angle_hi - o.angle_lo) * static_cast<Real>(i) / static_cast<Real>(o.angles - 1)) * phys::kPi / 180; };
    const auto speed_of = [&](int i) { return o.speed_lo + (o.speed_hi - o.speed_lo) * static_cast<Real>(i) / static_cast<Real>(o.speeds - 1); };

    std::vector<Node> beam{Node(Level{layout})};
    const unsigned threads = std::max(1u, std::thread::hardware_concurrency());
    const int depth = std::min<int>(o.depth, static_cast<int>(layout.ammo.size()));
    for (int d = 1; d <= depth && !beam.empty(); ++d) {
        std::vector<Trial> all;
        int best_wins = -1, best_robust = -1;
        size_t best_tried = 0;
        std::vector<Shot> best_solution;
        for (size_t b = 0; b < beam.size(); ++b) {
            const bool blast = beam[b].level.next_ammo() == Ammo::Blast;
            const std::vector<double> dets = blast ? o.detonate : std::vector<double>{0};
            std::vector<Trial> trials;
            for (int w = 0; w < static_cast<int>(o.waits.size()); ++w)
                for (int det = 0; det < static_cast<int>(dets.size()); ++det)
                    for (int ai = 0; ai < o.angles; ++ai)
                        for (int si = 0; si < o.speeds; ++si) trials.push_back({ai, si, w, det, std::nullopt, 0});
            std::atomic<size_t> next{0};
            std::vector<std::thread> pool;
            for (unsigned t = 0; t < threads; ++t)
                pool.emplace_back([&] {
                    for (size_t i; (i = next++) < trials.size();) {
                        Trial& tr = trials[i];
                        tr.result = beam[b];
                        if (!wait_for_turn(*tr.result, o.waits[static_cast<size_t>(tr.wait)])) { tr.score = -2; continue; }
                        play(*tr.result, tr.result->level.aim_for(angle_of(tr.angle), speed_of(tr.speed)), dets[static_cast<size_t>(tr.det)]);
                        tr.score = score(tr.result->level);
                    }
                });
            for (std::thread& t : pool) t.join();

            // Wins, and robust wins (the four grid neighbours win too, same wait and fuse).
            const auto won = [&](int ai, int si, int w, int det) {
                if (ai < 0 || si < 0 || ai >= o.angles || si >= o.speeds) return false;
                const size_t i = ((static_cast<size_t>(w) * dets.size() + static_cast<size_t>(det)) * static_cast<size_t>(o.angles) + static_cast<size_t>(ai)) *
                                     static_cast<size_t>(o.speeds) + static_cast<size_t>(si);
                return trials[i].result->level.status == Status::Won;
            };
            int wins = 0, robust = 0;
            const Trial* pick = nullptr;
            int pick_neighbours = -1;
            for (const Trial& tr : trials) {
                if (tr.result->level.status != Status::Won) continue;
                ++wins;
                const int n = won(tr.angle - 1, tr.speed, tr.wait, tr.det) + won(tr.angle + 1, tr.speed, tr.wait, tr.det) +
                              won(tr.angle, tr.speed - 1, tr.wait, tr.det) + won(tr.angle, tr.speed + 1, tr.wait, tr.det);
                if (n == 4) ++robust;
                if (n > pick_neighbours) {
                    pick_neighbours = n;
                    pick = &tr;
                }
            }
            if (wins > best_wins || (wins == best_wins && robust > best_robust)) {
                best_wins = wins;
                best_tried = trials.size();
                best_robust = robust;
                if (pick) best_solution = pick->result->shots;
            }
            for (Trial& tr : trials) all.push_back(std::move(tr));
        }
        std::printf("  %d shot%s: best state wins with %d of %zu tries, %.1f%% (%d robust)\n", d, d == 1 ? " " : "s", best_wins, best_tried,
                    best_tried ? 100.0 * best_wins / static_cast<double>(best_tried) : 0.0, best_robust);
        if (best_wins > 0) {
            std::printf("  solution:\n");
            for (const Shot& s : best_solution)
                std::printf("    {%ld, {%.9gf, %.9gf}, %ld},\n", s.step, static_cast<double>(s.aim.x), static_cast<double>(s.aim.y), s.detonate);
            std::printf("  that solution with every aim nudged up to 2 cm at random: %d of 100 win\n", jitter(layout, best_solution));
        }
        // Keep the best K distinct results for the next shot.
        std::sort(all.begin(), all.end(), [](const Trial& a, const Trial& b) { return a.score > b.score; });
        beam.clear();
        std::vector<double> seen;
        for (Trial& tr : all) {
            if (static_cast<int>(beam.size()) >= o.beam || tr.score < 0) break;
            if (tr.result->level.status != Status::Playing) continue;  // decided: nothing more to shoot at
            if (std::any_of(seen.begin(), seen.end(), [&](double s) { return std::fabs(s - tr.score) < 1e-6; })) continue;
            seen.push_back(tr.score);
            beam.push_back(std::move(*tr.result));
        }
        std::printf("      best after %d: %.1f targets down\n", d, beam.empty() ? 0.0 : std::floor(score(beam[0].level) / 100));
    }
    return 0;
}
