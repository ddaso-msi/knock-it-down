#pragma once
// How the world looks: the sky, hills and ground behind it, and the drawing of every kind of thing
// in it (blocks by material, pigs, birds, the sling). Everything is made of shapes and gradients:
// there are no images.
#include "render.hpp"

#include <level.hpp>

#include <optional>
#include <vector>

namespace kid::app {

// A palette and a mood. Levels 1-5 are a meadow, 6-10 snow, 11-15 dusk at the castle.
struct Theme {
    SDL_Color sky_top, sky_low, sun, cloud;
    SDL_Color far_hill, near_hill, grass, soil;
    SDL_Color rock, rock_edge;  // static scenery: platforms, walls, bunkers
    bool snow = false, castles = false;
    float sun_height = 8.5f;    // where the sun sits, in world metres
};
Theme theme_for(int level_index);

// The colour of a thing's main fill, for debris.
SDL_Color material_color(Material m);

// A line `width` pixels wide between two screen points.
void thick_line(SDL_Renderer* ren, SDL_FPoint a, SDL_FPoint b, float width, SDL_Color c);

// Sky, sun, clouds, distant hills (with parallax against the camera) and the ground. `time` drives
// the drift of the clouds.
void draw_backdrop(SDL_Renderer* ren, const Theme& t, double time);
// The ground strip in front: grass on top of the floor.
void draw_ground(SDL_Renderer* ren, const Theme& t);

// A soft dark copy under a body, a little down and to the right.
void draw_shadow(SDL_Renderer* ren, const Body& b);
// A block: shaded, with its material's surface (grain, speckle, glints). `seed` varies the details.
void draw_block(SDL_Renderer* ren, const Body& b, Material m, float condition, unsigned seed);
// Static scenery: rock with a lit top.
void draw_rock(SDL_Renderer* ren, const Body& b, const Theme& t);
// A pig, its eyes on `look_at`. Damaged pigs wince; every pig blinks now and then.
void draw_pig(SDL_Renderer* ren, const Body& b, Pig kind, float condition, Vec2 look_at, double time, unsigned seed);
// A bird of this kind: an ellipse along `heading`, `stretch` long and `squash` flattened, with eyes,
// brow, beak and tail.
void draw_bird(SDL_Renderer* ren, Vec2 pos, Real radius, Vec2 heading, Real stretch, Real squash, Ammo kind, bool egg);
// The slingshot: wood, the back rubber band and the pouch behind the bird (when there is one), then,
// after the bird is drawn, the front band over it.
void draw_sling(SDL_Renderer* ren, Vec2 pouch, std::optional<Vec2> bird, Real bird_radius, Vec2 wobble);
void draw_sling_front(SDL_Renderer* ren, Vec2 pouch, std::optional<Vec2> bird, Vec2 wobble);

// Snowflakes drifting down in front of the backdrop (only in snow themes).
class Weather {
public:
    void update(const Theme& t, float dt);
    void draw(SDL_Renderer* ren) const;

private:
    struct Flake {
        float x, y, speed, size, sway;
    };
    std::vector<Flake> flakes_;
    float time_ = 0;
};

}  // namespace kid::app
