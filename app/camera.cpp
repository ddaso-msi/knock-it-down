#include "camera.hpp"

#include <algorithm>
#include <cmath>

namespace kid::app {

float Camera::random() {
    seed_ = seed_ * 1664525u + 1013904223u;
    return static_cast<float>(seed_ >> 8) / static_cast<float>(1 << 23) - 1.0f;
}

void Camera::reset() {
    centre_ = home_centre();
    scale_ = kPixelsPerMeter;
    hold_ = shake_ = 0;
}

void Camera::shake(float pixels) { shake_ = std::max(shake_, pixels); }

void Camera::update(const Level& level, bool aiming, float dt) {
    Vec2 target = home_centre();
    float scale = kPixelsPerMeter;
    if (follow && !aiming && !level.shots.empty()) {
        // The bird to watch: the latest one while it is flying. A triple's three, or a dropper and its
        // egg, all count as that shot, so the one moving fastest is followed.
        const int shot = static_cast<int>(level.shots.size()) - 1;
        const Body* fastest = nullptr;
        for (size_t i = 0; i < level.world.bodies.size(); ++i) {
            if (level.shot_of(i) != shot || level.popped(static_cast<int>(i))) continue;
            const Body& b = level.world.bodies[i];
            if (!fastest || b.vel.length() > fastest->vel.length()) fastest = &b;
        }
        if (fastest && fastest->vel.length() > 1.5f && level.since_last_shot() < 7.0 && level.status == Status::Playing) {
            // Frame the bird (a little ahead of it) together with everything still standing.
            const Vec2 ahead = fastest->pos + fastest->vel * 0.2f;
            lo_ = hi_ = ahead;
            for (const int b : level.pigs)
                if (!level.popped(b)) {
                    lo_ = {std::min(lo_.x, level.world.bodies[static_cast<size_t>(b)].pos.x), std::min(lo_.y, kFloorTop)};
                    hi_ = {std::max(hi_.x, level.world.bodies[static_cast<size_t>(b)].pos.x), std::max(hi_.y, level.world.bodies[static_cast<size_t>(b)].pos.y)};
                }
            for (const int b : level.blocks)
                if (!level.popped(b)) {
                    const Vec2 p = level.world.bodies[static_cast<size_t>(b)].pos;
                    lo_ = {std::min(lo_.x, p.x), std::min(lo_.y, p.y)};
                    hi_ = {std::max(hi_.x, p.x), std::max(hi_.y, p.y)};
                }
            hold_ = kHoldTime;
        }
        if (hold_ > 0) {
            constexpr float kMargin = 1.2f;
            const float w = hi_.x - lo_.x + 2 * kMargin, h = hi_.y - lo_.y + 2 * kMargin;
            const float room_h = static_cast<float>(kHeight) - kTopBar - kFooter;
            scale = std::clamp(std::min(static_cast<float>(kWidth) / w, room_h / h), kPixelsPerMeter, kPixelsPerMeter * kZoom);
            // Centre the box in the part of the window the bars leave free.
            target = (lo_ + hi_) * 0.5f + Vec2{0, (kTopBar - kFooter) * 0.5f / scale};
            hold_ -= dt;
        }
    }
    // Ease towards the target: quickly in, a little slower on the zoom.
    centre_ += (target - centre_) * (1 - std::exp(-dt * 4.0f));
    scale_ += (scale - scale_) * (1 - std::exp(-dt * 3.0f));
    // Never show anything outside the arena.
    const float half_w = static_cast<float>(kWidth) * 0.5f / scale_, half_h = static_cast<float>(kHeight) * 0.5f / scale_;
    centre_.x = std::clamp(centre_.x, half_w, std::max(half_w, kWorldW - half_w));
    centre_.y = std::clamp(centre_.y, half_h, std::max(half_h, kWorldH - half_h));
    shake_ *= std::exp(-dt * 9.0f);
    if (shake_ < 0.3f) shake_ = 0;
}

void Camera::apply() { set_view(centre_, scale_, {shake_ * random(), shake_ * random()}); }

}  // namespace kid::app
