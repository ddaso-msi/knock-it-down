#pragma once
// The camera: the whole arena while you aim. Once a bird is away it frames the bird and everything
// still standing together, moving in as close as that allows (never closer than kZoom), so you see
// both where it is and what it is about to hit. It holds on the crash, then eases back out. It also
// shakes. All of this is only how the world is drawn: the simulation does not know about it.
#include "render.hpp"

#include <level.hpp>

#include <cstdint>

namespace kid::app {

class Camera {
public:
    static constexpr float kZoom = 1.4f;      // the closest it moves in
    // The window's top bar and footer cover part of the world; the framing keeps clear of them.
    static constexpr float kTopBar = 88.0f, kFooter = 32.0f;
    static constexpr float kHoldTime = 1.2f;  // seconds it stays where the bird came down

    bool follow = true;  // off: always the whole arena (for checking frames)

    // `aiming`: a bird is being pulled back, so the player needs to see everything.
    void update(const Level& level, bool aiming, float dt);
    // Adds a shake of about `pixels`, which dies away in a fraction of a second.
    void shake(float pixels);
    void reset();
    // Makes this the view the renderer draws with.
    void apply();

private:
    Vec2 centre_ = home_centre();
    float scale_ = kPixelsPerMeter;
    Vec2 lo_, hi_;  // the box being framed
    float hold_ = 0;
    float shake_ = 0;
    std::uint32_t seed_ = 777;

    static Vec2 home_centre() { return {kWorldW * 0.5f, kWorldH * 0.5f}; }
    float random();  // [-1, 1)
};

}  // namespace kid::app
