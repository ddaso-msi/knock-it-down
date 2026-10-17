#include "render.hpp"

#ifdef KID_TTF
#include <SDL3_ttf/SDL_ttf.h>

#include <map>
#include <optional>
#include <string>
#endif

#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace kid::app {

using phys::Body;
using phys::Vec2;

namespace {

constexpr int kSegments = 32;

struct View {
    Vec2 centre{static_cast<float>(kWidth) * 0.5f / kPixelsPerMeter, static_cast<float>(kHeight) * 0.5f / kPixelsPerMeter};
    float scale = kPixelsPerMeter;
    SDL_FPoint shake{0, 0};
};
View g_view;

SDL_FColor to_fcolor(SDL_Color c) {
    return {static_cast<float>(c.r) / 255.0f, static_cast<float>(c.g) / 255.0f, static_cast<float>(c.b) / 255.0f,
            static_cast<float>(c.a) / 255.0f};
}

// Convex outline as a triangle fan around its first point.
void fill_fan(SDL_Renderer* ren, const SDL_FPoint* pts, int count, SDL_Color fill) {
    SDL_Vertex verts[kSegments];
    int indices[(kSegments - 2) * 3];
    const SDL_FColor c = to_fcolor(fill);
    for (int i = 0; i < count; ++i) verts[i] = {pts[i], c, {0, 0}};
    for (int i = 0; i < count - 2; ++i) {
        indices[i * 3] = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = i + 2;
    }
    SDL_RenderGeometry(ren, nullptr, verts, count, indices, (count - 2) * 3);
}

void circle_points(SDL_FPoint centre, float radius, SDL_FPoint* out) {
    for (int i = 0; i < kSegments; ++i) {
        const float a = 2.0f * phys::kPi * static_cast<float>(i) / kSegments;
        out[i] = {centre.x + radius * std::cos(a), centre.y + radius * std::sin(a)};
    }
}

}  // namespace

void set_view(Vec2 centre, float pixels_per_meter, SDL_FPoint shake) { g_view = {centre, pixels_per_meter, shake}; }
float view_scale() { return g_view.scale; }
Vec2 view_centre() { return g_view.centre; }

float g_density = 1;
void set_pixel_density(SDL_Renderer* ren, float density) {
    g_density = density > 0 ? density : 1;
    SDL_SetRenderScale(ren, g_density, g_density);
}

SDL_FPoint to_screen(Vec2 w) {
    return {(w.x - g_view.centre.x) * g_view.scale + static_cast<float>(kWidth) * 0.5f + g_view.shake.x,
            (g_view.centre.y - w.y) * g_view.scale + static_cast<float>(kHeight) * 0.5f + g_view.shake.y};
}
Vec2 to_world(float sx, float sy) {
    return {(sx - g_view.shake.x - static_cast<float>(kWidth) * 0.5f) / g_view.scale + g_view.centre.x,
            g_view.centre.y - (sy - g_view.shake.y - static_cast<float>(kHeight) * 0.5f) / g_view.scale};
}

void set_color(SDL_Renderer* ren, SDL_Color c) { SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a); }

void fill_rect(SDL_Renderer* ren, const SDL_FRect& r, SDL_Color fill) {
    set_color(ren, fill);
    SDL_RenderFillRect(ren, &r);
}

void outline_rect(SDL_Renderer* ren, const SDL_FRect& r, SDL_Color edge) {
    set_color(ren, edge);
    SDL_RenderRect(ren, &r);
}

void fill_disc(SDL_Renderer* ren, SDL_FPoint centre, float radius, SDL_Color fill) {
    SDL_FPoint pts[kSegments];
    circle_points(centre, radius, pts);
    fill_fan(ren, pts, kSegments, fill);
}

void fill_polygon(SDL_Renderer* ren, const SDL_FPoint* pts, int count, SDL_Color fill) { fill_fan(ren, pts, count, fill); }

void fill_star(SDL_Renderer* ren, SDL_FPoint centre, float radius, SDL_Color fill) {
    // A fan from the centre works for a star: every point of it is visible from there.
    SDL_Vertex verts[11];
    int indices[30];
    const SDL_FColor c = to_fcolor(fill);
    verts[0] = {centre, c, {0, 0}};
    for (int i = 0; i < 10; ++i) {
        const float a = -phys::kPi * 0.5f + phys::kPi * static_cast<float>(i) / 5.0f;
        const float r = i % 2 == 0 ? radius : radius * 0.45f;
        verts[i + 1] = {{centre.x + r * std::cos(a), centre.y + r * std::sin(a)}, c, {0, 0}};
        indices[i * 3] = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = (i + 1) % 10 + 1;
    }
    SDL_RenderGeometry(ren, nullptr, verts, 11, indices, 30);
}

void draw_body(SDL_Renderer* ren, const Body& b, SDL_Color fill, SDL_Color edge) {
    SDL_FPoint pts[kSegments + 1];
    int count = 0;
    if (b.shape.type == phys::Shape::Type::Circle) {
        circle_points(to_screen(b.pos), to_pixels(b.shape.circle.radius), pts);
        count = kSegments;
    } else {
        const phys::Polygon& p = b.shape.polygon;
        for (int i = 0; i < p.count; ++i) pts[i] = to_screen(apply(b.transform(), p.vertices[i]));
        count = p.count;
    }
    fill_fan(ren, pts, count, fill);
    pts[count] = pts[0];
    set_color(ren, edge);
    SDL_RenderLines(ren, pts, count + 1);
    if (b.shape.type == phys::Shape::Type::Circle) {
        const SDL_FPoint c = to_screen(b.pos), tip = to_screen(b.pos + b.q.x_axis() * b.shape.circle.radius);
        SDL_RenderLine(ren, c.x, c.y, tip.x, tip.y);
    }
}

void dashed_line(SDL_Renderer* ren, Vec2 from, Vec2 to, SDL_Color c) {
    set_color(ren, c);
    constexpr float kDash = 0.2f;
    const float length = distance(from, to);
    const Vec2 dir = (to - from).normalized();
    for (float d = 0; d < length; d += 2 * kDash) {
        const SDL_FPoint a = to_screen(from + dir * d), b = to_screen(from + dir * std::fmin(d + kDash, length));
        SDL_RenderLine(ren, a.x, a.y, b.x, b.y);
    }
}

#ifdef KID_TTF
namespace {

// A real typeface. Each piece of text is rendered once into a texture at the screen's full pixel
// density and kept while it keeps being drawn: most text (labels, counters) repeats frame after frame.
struct Font {
    std::string path;
    std::map<int, TTF_Font*> sizes;  // by pixel size
};
std::optional<Font> g_font;
struct Cached {
    SDL_Texture* texture;
    float w, h;  // in window coordinates
    Uint64 used;
};
std::map<std::string, Cached> g_text_cache;
Uint64 g_text_frame = 0;

// The debug font's glyphs are 8 pixels a scale step; this face, a little larger, reads the same size.
int pixel_size(float scale) { return std::max(6, static_cast<int>(std::lround(9.5f * scale * g_density))); }

TTF_Font* font_for(float scale) {
    const int px = pixel_size(scale);
    auto it = g_font->sizes.find(px);
    if (it != g_font->sizes.end()) return it->second;
    TTF_Font* f = TTF_OpenFont(g_font->path.c_str(), static_cast<float>(px));
    return g_font->sizes[px] = f;
}

}  // namespace

bool load_font(std::initializer_list<const char*> paths) {
    if (!TTF_Init()) return false;
    for (const char* p : paths) {
        TTF_Font* f = TTF_OpenFont(p, 16);
        if (!f) continue;
        TTF_CloseFont(f);
        g_font = Font{p, {}};
        return true;
    }
    return false;
}

void close_font() {
    for (auto& [key, c] : g_text_cache) SDL_DestroyTexture(c.texture);
    g_text_cache.clear();
    if (g_font)
        for (auto& [px, f] : g_font->sizes)
            if (f) TTF_CloseFont(f);
    g_font.reset();
    TTF_Quit();
}

void end_text_frame() {
    ++g_text_frame;
    for (auto it = g_text_cache.begin(); it != g_text_cache.end();)
        if (g_text_frame - it->second.used > 120) {
            SDL_DestroyTexture(it->second.texture);
            it = g_text_cache.erase(it);
        } else {
            ++it;
        }
}
#else
bool load_font(std::initializer_list<const char*>) { return false; }
void close_font() {}
void end_text_frame() {}
#endif

void text(SDL_Renderer* ren, float x, float y, float scale, SDL_Color c, const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
#ifdef KID_TTF
    if (g_font && buf[0]) {
        if (TTF_Font* f = font_for(scale)) {
            char key[300];
            std::snprintf(key, sizeof(key), "%d|%d,%d,%d|%s", pixel_size(scale), c.r, c.g, c.b, buf);
            auto it = g_text_cache.find(key);
            if (it == g_text_cache.end()) {
                SDL_Surface* surf = TTF_RenderText_Blended(f, buf, 0, {c.r, c.g, c.b, 255});
                SDL_Texture* tex = surf ? SDL_CreateTextureFromSurface(ren, surf) : nullptr;
                if (tex) it = g_text_cache.emplace(key, Cached{tex, static_cast<float>(surf->w) / g_density, static_cast<float>(surf->h) / g_density, 0}).first;
                SDL_DestroySurface(surf);
            }
            if (it != g_text_cache.end()) {
                it->second.used = g_text_frame;
                SDL_SetTextureAlphaMod(it->second.texture, c.a);  // fading text shares one image
                // Centred on the line the pixel font would have used, which is 8 * scale tall.
                const SDL_FRect dst{x, y + (8.0f * scale - it->second.h) * 0.5f, it->second.w, it->second.h};
                SDL_RenderTexture(ren, it->second.texture, nullptr, &dst);
                return;
            }
        }
    }
#endif
    set_color(ren, c);
    SDL_SetRenderScale(ren, scale * g_density, scale * g_density);
    SDL_RenderDebugText(ren, x / scale, y / scale, buf);
    SDL_SetRenderScale(ren, g_density, g_density);
}

float text_width(const char* str, float scale) {
#ifdef KID_TTF
    if (g_font && str[0])
        if (TTF_Font* f = font_for(scale)) {
            int w = 0, h = 0;
            if (TTF_GetStringSize(f, str, 0, &w, &h)) return static_cast<float>(w) / g_density;
        }
#endif
    return static_cast<float>(std::strlen(str)) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
}

std::string spaced(const std::string& s) {
    std::string out;
    for (const char ch : s) {
        out += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        out += ' ';
    }
    if (!out.empty()) out.pop_back();
    return out;
}

void text_centered(SDL_Renderer* ren, float cx, float y, float scale, SDL_Color c, const char* str) {
    text(ren, cx - text_width(str, scale) * 0.5f, y, scale, c, "%s", str);
}

void draw_button(SDL_Renderer* ren, const Button& b, bool hovered, bool enabled) {
    // Just the words, spaced; a thin line under them when the pointer is on them.
    const std::string label = spaced(b.label);
    const SDL_Color c = !enabled ? SDL_Color{196, 178, 160, 90} : hovered ? color::kTarget : color::kDim;
    text_centered(ren, b.rect.x + b.rect.w * 0.5f, b.rect.y + (b.rect.h - 16.0f) * 0.5f, 1.6f, c, label.c_str());
    if (hovered && enabled) fill_rect(ren, {b.rect.x + 14.0f, b.rect.y + b.rect.h - 6.0f, b.rect.w - 28.0f, 1.5f}, color::kTarget);
}

}  // namespace kid::app
