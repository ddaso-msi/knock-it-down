#pragma once
// Draws a level: its bodies coloured by role and material, its joints, and the marks it asks for.
#include "render.hpp"

#include <level.hpp>

namespace kid::app {

struct RoleColors {
    SDL_Color fill, edge;
};
RoleColors role_colors(Role role);

void draw_grid(SDL_Renderer* ren);
void draw_level(SDL_Renderer* ren, const Level& level, Vec2 pointer, bool pointer_active, bool show_contacts);
// A small picture of one shot type, for the ammo queue.
void draw_ammo_icon(SDL_Renderer* ren, SDL_FPoint centre, Ammo a, bool spent);

}  // namespace kid::app
