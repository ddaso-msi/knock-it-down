#pragma once
// Draws a level: its bodies coloured by role and material, its joints, and the marks it asks for.
#include "render.hpp"

#include <level.hpp>

#include <optional>

namespace kid::app {

struct RoleColors {
    SDL_Color fill, edge;
};
RoleColors role_colors(Role role);
// The colours a body is drawn in: by role, and for blocks by what they are made of.
RoleColors body_colors(Role role, Material m);
class Effects;

void draw_grid(SDL_Renderer* ren);
// `pull`: the bird is being held back in the sling.
// `fx`, if given, draws its trails under the bodies.
struct Theme;
void draw_level(SDL_Renderer* ren, const Level& level, std::optional<Vec2> pull, bool show_contacts, const Effects* fx, const Theme& theme);
// A small picture of one shot type, for the ammo queue.
void draw_ammo_icon(SDL_Renderer* ren, SDL_FPoint centre, Ammo a, bool spent);

}  // namespace kid::app
