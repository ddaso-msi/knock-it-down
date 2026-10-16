#pragma once
// Plain questions about the simulation state, asked by the rules after each step and by the tests about
// hand-built worlds.
#include <phys/world.hpp>

#include <cmath>
#include <span>

namespace kid {

// Are all of `bodies` (indices into w.bodies) slower than both limits (m/s and rad/s)? A sleeping
// body counts as slow, and so does an empty list.
inline bool all_slow(const phys::World& w, std::span<const int> bodies, phys::Real linear = 0.1f, phys::Real angular = 0.2f) {
    for (int i : bodies) {
        const phys::Body& b = w.bodies[static_cast<size_t>(i)];
        if (b.vel.length() > linear || std::fabs(b.w) > angular) return false;
    }
    return true;
}

}  // namespace kid
