#include "render.hpp"

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

void text(SDL_Renderer* ren, float x, float y, float scale, SDL_Color c, const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    set_color(ren, c);
    SDL_SetRenderScale(ren, scale * g_density, scale * g_density);
    SDL_RenderDebugText(ren, x / scale, y / scale, buf);
    SDL_SetRenderScale(ren, g_density, g_density);
}

float text_width(const char* str, float scale) {
    return static_cast<float>(std::strlen(str)) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
}

void text_centered(SDL_Renderer* ren, float cx, float y, float scale, SDL_Color c, const char* str) {
    text(ren, cx - text_width(str, scale) * 0.5f, y, scale, c, "%s", str);
}

void draw_button(SDL_Renderer* ren, const Button& b, bool hovered, bool enabled) {
    fill_rect(ren, b.rect, hovered && enabled ? SDL_Color{52, 60, 82, 255} : SDL_Color{36, 41, 56, 255});
    outline_rect(ren, b.rect, hovered && enabled ? color::kToolEdge : color::kPanelEdge);
    text_centered(ren, b.rect.x + b.rect.w * 0.5f, b.rect.y + (b.rect.h - 16.0f) * 0.5f, 2.0f,
                  enabled ? color::kText : color::kDim, b.label);
}

}  // namespace kid::app
