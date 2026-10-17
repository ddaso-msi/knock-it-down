#pragma once
// How the world looks: backlit silhouettes. A glowing sky low behind the target, hills fading into
// haze, and everything solid drawn dark, rimmed with light on the side facing the sun. Colour is kept
// for what matters: the pigs' eyes, the birds' streaks, ice, fire. Everything is made of shapes and
// gradients: there are no images.
#include "render.hpp"

#include <level.hpp>

#include <optional>
#include <vector>

namespace kid::app {

// A mood. Levels 1-5 are amber dusk, 6-10 a cold moonlit night with snow, 11-15 embers at the castle.
struct Theme {
    SDL_Color sky_top, sky_mid, sky_low;
    SDL_Color sun;   // the light source, and the colour of every rim
    SDL_Color haze;  // the fog between the hills
    SDL_Color eyes;  // the pigs' glow
    Vec2 sun_at;     // where it sits, in world metres (behind the structures)
    float sun_size;  // its radius, in metres
    bool snow = false, castles = false, embers = false;
};
Theme theme_for(int level_index);

constexpr SDL_Color kInk{10, 10, 14, 255};  // the colour of every silhouette

// The colour debris of this material flies as.
SDL_Color material_color(Material m);

// A line `width` pixels wide between two screen points.
void thick_line(SDL_Renderer* ren, SDL_FPoint a, SDL_FPoint b, float width, SDL_Color c);
// A soft glow: `inner` at the centre fading to nothing at `radius` pixels.
void glow(SDL_Renderer* ren, SDL_FPoint centre, float radius, SDL_Color inner);

// Sky, sun, stars, the hills in layers of haze (with parallax against the camera), castles.
void draw_backdrop(SDL_Renderer* ren, const Theme& t, double time);
// The ground: ink, with grass blades along its edge.
void draw_ground(SDL_Renderer* ren, const Theme& t);
// Darkened edges, last thing before the interface.
void draw_vignette(SDL_Renderer* ren);
// A rectangle shading from one colour at the top to another at the bottom (screen pixels).
void gradient_rect(SDL_Renderer* ren, SDL_FRect r, SDL_Color top, SDL_Color bottom);

// A block: a dark shape in its material's tone, rimmed where it faces the light. Cracks glow.
void draw_block(SDL_Renderer* ren, const Body& b, Material m, float condition, unsigned seed, const Theme& t);
// Static scenery: pure ink, rimmed.
void draw_rock(SDL_Renderer* ren, const Body& b, const Theme& t);
// A pig: a dark round shape with two glowing eyes on `look_at`. Hurt pigs' eyes burn orange; every
// pig blinks now and then.
void draw_pig(SDL_Renderer* ren, const Body& b, Pig kind, float condition, Vec2 look_at, double time, unsigned seed, const Theme& t);
// A bird: a dark streamlined shape along `heading`, `stretch` long and `squash` flattened, with one
// bright eye and a streak in its kind's colour.
void draw_bird(SDL_Renderer* ren, Vec2 pos, Real radius, Vec2 heading, Real stretch, Real squash, Ammo kind, bool egg, const Theme& t);
// The colour that says which bird it is.
SDL_Color bird_color(Ammo kind);
// The slingshot: dark wood and bands, the pouch behind the bird; then, after the bird is drawn, the
// front band over it.
void draw_sling(SDL_Renderer* ren, Vec2 pouch, std::optional<Vec2> bird, Real bird_radius, Vec2 wobble, const Theme& t);
void draw_sling_front(SDL_Renderer* ren, Vec2 pouch, std::optional<Vec2> bird, Vec2 wobble);

// What drifts in the air: dust in the light always, snow on cold nights, sparks rising at the castle.
class Weather {
public:
    void update(const Theme& t, float dt);
    void draw(SDL_Renderer* ren, const Theme& t) const;

private:
    struct Mote {
        float x, y, speed, size, sway;
    };
    std::vector<Mote> motes_;
    float time_ = 0;
    int kind_ = -1;  // which theme the motes were made for
};

}  // namespace kid::app
