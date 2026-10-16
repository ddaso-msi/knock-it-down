#include "test.hpp"

#include <objectives.hpp>

using namespace kid;
using namespace phys;

TEST(all_slow_needs_every_listed_body_under_both_limits) {
    World w;
    const int a = w.add(Body(Shape::make_circle(0.5f), {0, 0}, 0));
    const int b = w.add(Body(Shape::make_circle(0.5f), {3, 0}, 0));
    const int both[] = {a, b}, only_a[] = {a};
    CHECK(all_slow(w, both));
    w.bodies[static_cast<size_t>(b)].vel = {0.2f, 0};
    CHECK(!all_slow(w, both));
    CHECK(all_slow(w, only_a));  // unlisted bodies do not count
    w.bodies[static_cast<size_t>(b)].vel = {};
    w.bodies[static_cast<size_t>(b)].w = 0.3f;
    CHECK(!all_slow(w, both));
    CHECK(all_slow(w, both, 0.1f, 0.5f));
    CHECK(all_slow(w, std::span<const int>{}));
}
