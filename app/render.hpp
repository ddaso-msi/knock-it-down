#pragma once
// Drawing helpers for the game: world-to-screen mapping, filled shapes, scaled text, buttons.
#include <SDL3/SDL.h>
#include <phys/world.hpp>

#include <initializer_list>
#include <string>

namespace kid::app {

constexpr int kWidth = 1280, kHeight = 720;
constexpr float kPixelsPerMeter = 64.0f;  // at this zoom the window shows the 20 x 11.25 m arena exactly

// The view of the world: the world point at the middle of the window, pixels per metre, and a shake
// offset in pixels. The camera sets it each frame; until then it shows the whole arena.
void set_view(phys::Vec2 centre, float pixels_per_meter, SDL_FPoint shake = {0, 0});
float view_scale();
phys::Vec2 view_centre();
// Draw at the window's real pixel density (2 on a Retina screen), in the same 1280 x 720 coordinates.
void set_pixel_density(SDL_Renderer* ren, float density);
SDL_FPoint to_screen(phys::Vec2 w);
phys::Vec2 to_world(float sx, float sy);
// A length in metres, in pixels at the current zoom.
inline float to_pixels(float meters) { return meters * view_scale(); }

// The palette. Targets are the only warm colour on screen.
namespace color {  // warm light on near-black: the interface speaks quietly and in the scene's own light
constexpr SDL_Color kBackground{10, 10, 14, 255}, kGrid{24, 22, 28, 255};
constexpr SDL_Color kStatic{58, 65, 84, 255}, kStaticEdge{126, 136, 162, 255};
constexpr SDL_Color kTarget{255, 190, 110, 255}, kTargetEdge{255, 214, 160, 255};
constexpr SDL_Color kDone{70, 122, 100, 255}, kDoneEdge{120, 200, 160, 255};
constexpr SDL_Color kTool{74, 170, 232, 255}, kToolEdge{255, 214, 160, 255};
constexpr SDL_Color kText{244, 232, 216, 255}, kDim{196, 178, 160, 200};
constexpr SDL_Color kPanel{8, 8, 12, 190}, kPanelEdge{120, 96, 80, 160};
constexpr SDL_Color kGood{196, 236, 150, 255}, kBad{255, 120, 90, 255};
}  // namespace color

void set_color(SDL_Renderer* ren, SDL_Color c);
void fill_rect(SDL_Renderer* ren, const SDL_FRect& r, SDL_Color fill);
void outline_rect(SDL_Renderer* ren, const SDL_FRect& r, SDL_Color edge);
// A disc given in screen pixels.
void fill_disc(SDL_Renderer* ren, SDL_FPoint centre, float radius, SDL_Color fill);
// A convex polygon given in screen pixels (at most 32 points).
void fill_polygon(SDL_Renderer* ren, const SDL_FPoint* pts, int count, SDL_Color fill);
// A five-pointed star given in screen pixels (`radius` to the points).
void fill_star(SDL_Renderer* ren, SDL_FPoint centre, float radius, SDL_Color fill);
// A body, filled and outlined. Circles get a spoke so spin is visible.
void draw_body(SDL_Renderer* ren, const phys::Body& b, SDL_Color fill, SDL_Color edge);
// A dashed line between two world points.
void dashed_line(SDL_Renderer* ren, phys::Vec2 from, phys::Vec2 to, SDL_Color c);

// Text in SDL's built-in 8x8 font, enlarged `scale` times. (x, y) is the top-left corner in pixels.
void text(SDL_Renderer* ren, float x, float y, float scale, SDL_Color c, SDL_PRINTF_FORMAT_STRING const char* fmt, ...)
    SDL_PRINTF_VARARG_FUNC(6);
void text_centered(SDL_Renderer* ren, float cx, float y, float scale, SDL_Color c, const char* str);
float text_width(const char* str, float scale);
// Capitals with a space between letters: how labels are written in this style.
std::string spaced(const std::string& s);
// Switches text to the first of these font files that opens (when built with SDL3_ttf). Returns
// whether one did; otherwise text stays in SDL's built-in pixel font. close_font() before the renderer goes.
bool load_font(std::initializer_list<const char*> paths);
void close_font();
// Lets go of text images not drawn for a couple of seconds. Call once a frame.
void end_text_frame();

struct Button {
    SDL_FRect rect;
    const char* label;
    bool hit(float x, float y) const {
        return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
    }
};
void draw_button(SDL_Renderer* ren, const Button& b, bool hovered, bool enabled = true);

}  // namespace kid::app
