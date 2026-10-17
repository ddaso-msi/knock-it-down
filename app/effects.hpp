#pragma once
// What the game draws on top of the physics that is not physics: debris from what pops, the points
// floating up, the path the last bird took, and a marker for a bird above the top of the screen.
// None of it touches the simulation, so it can use real time and its own random numbers.
#include "render.hpp"

#include <level.hpp>

#include <cstdint>
#include <utility>
#include <vector>

namespace kid::app {

class Effects {
public:
    // Catch up with the level after it has stepped: pops become debris and points, the bird in flight
    // adds to its trail. `dt` is the (possibly slowed) seconds of animation to advance.
    void update(const Level& level, float dt);
    // Points floating up from each bird left over, for the win.
    void bird_bonus(Vec2 at, int points);
    void clear();
    // Trails go under the bodies; debris, points and markers go over them.
    void draw_under(SDL_Renderer* ren) const;
    void draw_over(SDL_Renderer* ren, const Level& level, float top_of_view) const;
    // How squashed a bird is from its latest hit: 1 just hit, 0 back to round.
    float squash(int body) const;
    // Where the empty sling's bands meet, relative to the pouch, as they quiver after a shot.
    Vec2 sling_wobble() const;

private:
    struct Bit {
        Vec2 p, v;
        Real angle, spin, size;
        float life, max_life;
        SDL_Color color;
        int shape;  // 0 a chunk (stone), 1 a splinter (wood), 2 a shard (ice), 3 an ember
    };
    struct Puff {  // smoke where a pig was
        Vec2 p;
        Real size;
        float life;
    };
    std::vector<Puff> puffs_;
    struct Points {
        Vec2 p;
        int value;
        float life;
        SDL_Color color;
    };
    float random();  // in [0, 1)

    std::vector<Bit> bits_;
    std::vector<Points> points_;
    std::vector<Vec2> trail_, last_trail_;
    std::vector<std::pair<int, float>> squash_;  // bird body, time left
    size_t hits_seen_ = 0;
    float wobble_t_ = 10;  // seconds since the last shot left the sling
    Vec2 wobble_dir_;
    size_t pops_seen_ = 0;
    size_t shots_seen_ = 0;
    double last_mark_ = -1;      // sim_time of the latest trail dot
    double level_time_ = 0;      // to notice a reset
    std::uint32_t seed_ = 12345;
};

}  // namespace kid::app
