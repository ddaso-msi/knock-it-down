// Sling: pull a bird back in the slingshot and let go, into a live physics world, until every pig has popped.
//
//   press near the sling, drag back, let go  fire (the dots show the path)
//   click while a blast bird is in flight    set it off       right click or Esc while pulling: let it down
//   R  reset the level     Tab  pause     H  help     C  show contacts     N  next level (after solving)
//   Esc  close the help, then back to the menu
//   On the menu: click a level, or arrows + Enter, or 1-9.     Q  quit
//
// Best results are saved to the user's preferences folder (SDL_GetPrefPath), one line per level.
//
// Checking a build without a display: arguments are run in order as a script, then the program exits.
// Nothing is saved in this mode.
//   --level N          open level N
//   --mouse X Y        put the cursor at a world position (metres)
//   --press X Y        press the left button there      --drag X Y   move with it held
//   --release X Y      let go there                     --click X Y  press and release
//   --shoot DEG SPEED  fire at this angle and speed (the solver's way of saying where)
//   --steps N          advance the simulation N fixed steps
//   --key NAME         reset, pause, help, contacts, next, escape
//   --capture FILE     draw the current frame and save it as a BMP
#include "effects.hpp"
#include "render.hpp"
#include "scene.hpp"

#include <SDL3/SDL.h>
#include <levels/levels.hpp>
#include <phys/timestep.hpp>
#include <progress.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string>

using namespace kid;
using namespace kid::app;
using phys::Vec2;

namespace {

constexpr float kBarHeight = 88.0f;
constexpr float kFooterTop = static_cast<float>(kHeight) - kFloorTop * kPixelsPerMeter;
constexpr int kColumns = 5;
// When the last pig pops, the game runs at kSlowSpeed for kSlowTime seconds before the win panel.
constexpr float kSlowTime = 1.4f, kSlowSpeed = 0.3f;

enum class Action { None, Reset, Pause, Help, Menu, Next, Contacts };

struct App {
    std::unique_ptr<Level> level;  // null on the menu
    int index = 0;                 // which level `level` is, or the selected card on the menu
    bool paused = false;
    bool show_help = false;
    bool show_contacts = false;
    bool running = true;
    float mouse_x = 0, mouse_y = 0;
    phys::FixedTimestep timestep{kTimeStep};
    Progress progress;
    std::optional<Vec2> pull;      // a bird is being held back in the sling: by this much
    Effects fx;
    float slow = 0;                // seconds of slow motion still to go
    bool scripted = false;         // the scripted run: every update is exactly one step, never slowed
    std::string save_path;         // empty: do not save (script mode)
    bool recorded = false;         // this attempt's win is already in `progress`
};

struct BarButton {
    Button button;
    Action action;
};
constexpr BarButton kButtons[] = {
    {{{636.0f, 10.0f, 136.0f, 36.0f}, "R Reset"}, Action::Reset},
    {{{780.0f, 10.0f, 176.0f, 36.0f}, "Tab Pause"}, Action::Pause},
    {{{964.0f, 10.0f, 120.0f, 36.0f}, "H Help"}, Action::Help},
    {{{1092.0f, 10.0f, 168.0f, 36.0f}, "Esc Menu"}, Action::Menu},
};

SDL_FRect card_rect(int i) {
    const int col = i % kColumns, row = i / kColumns;
    return {60.0f + 236.0f * static_cast<float>(col), 220.0f + 128.0f * static_cast<float>(row), 216.0f, 112.0f};
}
bool inside(const SDL_FRect& r, float x, float y) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; }
bool over_world(const App& app) { return app.mouse_y > kBarHeight && app.mouse_y < kFooterTop; }

// ---- saving ----

void load_progress(App& app) {
    char* dir = SDL_GetPrefPath("Deb AI Labs", "Sling");
    if (!dir) return;
    app.save_path = std::string(dir) + "progress.txt";
    SDL_free(dir);
    size_t size = 0;
    if (void* data = SDL_LoadFile(app.save_path.c_str(), &size)) {
        app.progress = parse_progress(std::string(static_cast<const char*>(data), size));
        SDL_free(data);
    }
}

void save_progress(const App& app) {
    if (app.save_path.empty()) return;
    const std::string text = format_progress(app.progress);
    if (!SDL_SaveFile(app.save_path.c_str(), text.data(), text.size())) SDL_Log("could not save progress: %s", SDL_GetError());
}

// ---- state changes ----

void start_level(App& app, int index) {
    if (index < 0 || index >= level_count()) return;
    app.level = std::make_unique<Level>(make_layout(index));
    app.index = index;
    app.paused = app.show_help = app.recorded = false;
    app.pull.reset();
    app.fx.clear();
    app.slow = 0;
    app.timestep = phys::FixedTimestep(kTimeStep);
}

void leave_level(App& app) {
    app.level.reset();
    app.pull.reset();
}

void apply(App& app, Action action) {
    Level& level = *app.level;
    switch (action) {
        case Action::Reset:
            level.reset();
            app.paused = app.recorded = false;
            app.pull.reset();
            app.fx.clear();
            app.slow = 0;
            app.timestep = phys::FixedTimestep(kTimeStep);
            break;
        case Action::Pause: app.paused = !app.paused; break;
        case Action::Help: app.show_help = !app.show_help; break;
        case Action::Contacts: app.show_contacts = !app.show_contacts; break;
        case Action::Next:  // only once solved; after the last level, back to the menu
            if (level.status != Status::Won) break;
            if (app.index + 1 < level_count()) start_level(app, app.index + 1);
            else leave_level(app);
            break;
        case Action::Menu:  // Esc backs out one thing at a time: the help, a pulled bird, then the level
            if (app.show_help) app.show_help = false;
            else if (app.pull) app.pull.reset();
            else leave_level(app);
            break;
        case Action::None: break;
    }
}

void press(App& app, float x, float y) {
    app.mouse_x = x;
    app.mouse_y = y;
    if (!app.level) {
        for (int i = 0; i < level_count(); ++i)
            if (inside(card_rect(i), x, y)) start_level(app, i);
        return;
    }
    if (app.show_help) {  // any click dismisses the help and does nothing else
        app.show_help = false;
        return;
    }
    for (const BarButton& b : kButtons)
        if (b.button.hit(x, y)) {
            apply(app, b.action);
            return;
        }
    if (!over_world(app) || app.paused) return;
    Level& level = *app.level;
    if (level.power()) return;  // a click while a bird can still use its power uses it
    // Grab the bird: anywhere near the sling counts, it does not have to be a precise click.
    constexpr Real kGrab = 1.5f;
    const Vec2 p = to_world(x, y);
    if (level.can_fire() && distance(p, level.layout().pouch) < kGrab) app.pull = level.pull_to(p);
}

void move(App& app, float x, float y) {
    app.mouse_x = x;
    app.mouse_y = y;
    if (app.level && app.pull) app.pull = app.level->pull_to(to_world(x, y));
}

void release(App& app, float x, float y) {
    move(app, x, y);
    if (!app.level || !app.pull) return;
    app.level->fire(*app.pull);  // a pull too short to count just lets the bird back down
    app.pull.reset();
}

void key(App& app, SDL_Keycode k) {
    if (!app.level) {
        const int n = level_count();
        if (k == SDLK_Q) app.running = false;
        if (k == SDLK_RETURN || k == SDLK_SPACE) start_level(app, app.index);
        if (k == SDLK_RIGHT) app.index = (app.index + 1) % n;
        if (k == SDLK_LEFT) app.index = (app.index + n - 1) % n;
        if (k == SDLK_DOWN) app.index = std::min(n - 1, app.index + kColumns);
        if (k == SDLK_UP) app.index = std::max(0, app.index - kColumns);
        if (k >= SDLK_1 && k <= SDLK_9) start_level(app, static_cast<int>(k - SDLK_1));
        return;
    }
    switch (k) {
        case SDLK_R: apply(app, Action::Reset); break;
        case SDLK_TAB: apply(app, Action::Pause); break;
        case SDLK_H: apply(app, Action::Help); break;
        case SDLK_C: apply(app, Action::Contacts); break;
        case SDLK_N: apply(app, Action::Next); break;
        case SDLK_ESCAPE: apply(app, Action::Menu); break;
        default: break;
    }
}

void handle_event(App& app, const SDL_Event& e) {
    switch (e.type) {
        case SDL_EVENT_QUIT: app.running = false; break;
        case SDL_EVENT_KEY_DOWN:
            if (!e.key.repeat) key(app, e.key.key);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            move(app, e.motion.x, e.motion.y);
            if (!app.level)
                for (int i = 0; i < level_count(); ++i)
                    if (inside(card_rect(i), app.mouse_x, app.mouse_y)) app.index = i;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:  // one event per press: holding the button does not repeat
            if (e.button.button == SDL_BUTTON_LEFT) press(app, e.button.x, e.button.y);
            if (e.button.button == SDL_BUTTON_RIGHT) app.pull.reset();
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (e.button.button == SDL_BUTTON_LEFT) release(app, e.button.x, e.button.y);
            break;
        default: break;
    }
}

void update(App& app, phys::Real frame_seconds) {
    // Paused, on the menu or reading the help: bank no time, so play resumes exactly where it stopped.
    if (!app.level || app.paused || app.show_help) return;
    const float speed = app.slow > 0 && !app.scripted ? kSlowSpeed : 1.0f;
    app.slow = std::max(0.0f, app.slow - frame_seconds);
    app.timestep.advance(frame_seconds * speed, [&](phys::Real) { app.level->step(); });
    app.fx.update(*app.level, frame_seconds * speed);
    if (app.level->status == Status::Won && !app.recorded) {
        app.recorded = true;
        app.slow = kSlowTime;
        // The birds left over each count up from the sling.
        const Level& level = *app.level;
        for (int i = 0; i < level.shots_left(); ++i)
            app.fx.bird_bonus(level.layout().pouch + Vec2{0, 1.0f + 0.6f * static_cast<float>(i)}, Level::kBirdBonus);
        if (app.progress.record(app.level->name(), app.level->stars(), app.level->score)) save_progress(app);
    }
}

// ---- drawing ----

void draw_panel(SDL_Renderer* ren, const SDL_FRect& r, SDL_Color edge) {
    fill_rect(ren, r, color::kPanel);
    outline_rect(ren, r, edge);
    outline_rect(ren, {r.x + 1, r.y + 1, r.w - 2, r.h - 2}, edge);
}

// `count` stars of which `lit` are filled, centred on (cx, cy).
void draw_stars(SDL_Renderer* ren, float cx, float cy, float size, int lit, int count = 3) {
    const float pitch = size * 2.4f;
    for (int i = 0; i < count; ++i) {
        const SDL_FPoint c{cx + (static_cast<float>(i) - static_cast<float>(count - 1) * 0.5f) * pitch, cy};
        fill_star(ren, c, size, i < lit ? color::kTarget : SDL_Color{58, 64, 82, 255});
    }
}

void draw_hud(SDL_Renderer* ren, const App& app) {
    const Level& level = *app.level;
    const float width = static_cast<float>(kWidth), cx = width * 0.5f;
    fill_rect(ren, {0, 0, width, kBarHeight}, color::kPanel);
    set_color(ren, color::kPanelEdge);
    SDL_RenderLine(ren, 0, kBarHeight, width, kBarHeight);

    text(ren, 20.0f, 20.0f, 2.0f, color::kText, "%d %s", app.index + 1, level.name().c_str());
    const char* mode = app.paused ? "PAUSED" : level.status == Status::Won ? "SOLVED" : level.status == Status::Failed ? "FAILED" : "PLAYING";
    const SDL_Color mode_color = app.paused ? color::kTarget : level.status == Status::Won ? color::kGood : level.status == Status::Failed ? color::kBad : color::kText;
    const SDL_FRect pill{460.0f, 12.0f, 140.0f, 32.0f};
    outline_rect(ren, pill, mode_color);
    text_centered(ren, pill.x + pill.w * 0.5f, 20.0f, 2.0f, mode_color, mode);
    for (const BarButton& b : kButtons) {
        Button button = b.button;
        if (b.action == Action::Pause && app.paused) button.label = "Tab Resume";
        draw_button(ren, button, button.hit(app.mouse_x, app.mouse_y));
    }

    text(ren, 20.0f, 60.0f, 2.0f, color::kDim, "%s", level.layout().objective.c_str());
    const std::string stats = level.stats();
    text(ren, width - 20.0f - text_width(stats.c_str(), 2.0f), 60.0f, 2.0f, color::kText, "%s", stats.c_str());

    // The shots, in firing order, just under the bar: the next one first and bright.
    const std::vector<Ammo>& ammo = level.layout().ammo;
    float x = 34.0f;
    for (size_t i = 0; i < ammo.size(); ++i) {
        draw_ammo_icon(ren, {x, kBarHeight + 30.0f}, ammo[i], static_cast<int>(i) < level.shots_used());
        x += 44.0f;
    }
    if (level.next_ammo()) text(ren, x, kBarHeight + 22.0f, 2.0f, color::kDim, "next: %s", ammo_props(*level.next_ammo()).name);

    // The floor strip doubles as a footer: what to do now on the left, the counters on the right.
    const std::string hint = level.hint();
    text(ren, 48.0f, kFooterTop + 8.0f, 2.0f, color::kText, "%s", hint.c_str());
    // The counters on the right, as many as fit beside the hint: the time goes first, then the best score.
    const Best best = app.progress.best(level.name());
    const int three = level.layout().stars.three;
    char counters[96];
    const float room = width - 96.0f - text_width(hint.c_str(), 2.0f) - 32.0f;
    for (int detail = 2; detail >= 0; --detail) {
        int n = SDL_snprintf(counters, sizeof(counters), "3 stars %d", three);
        if (detail >= 1 && best.stars > 0) n += SDL_snprintf(counters + n, sizeof(counters) - static_cast<size_t>(n), "   Best %d", best.score);
        n += SDL_snprintf(counters + n, sizeof(counters) - static_cast<size_t>(n), "   Try %d", level.attempt);
        if (detail >= 2) SDL_snprintf(counters + n, sizeof(counters) - static_cast<size_t>(n), "   %5.1f s", level.sim_time);
        if (text_width(counters, 2.0f) <= room) break;
    }
    text(ren, width - 48.0f - text_width(counters, 2.0f), kFooterTop + 8.0f, 2.0f, color::kDim, "%s", counters);

    if (app.show_help) {
        const std::vector<std::string> lines = level.help();
        const char* keys[] = {
            "Drag   pull a bird back from the sling; let go to fire",
            "Click  use the power of the bird in flight (before it hits anything)",
            "RMB    let a pulled bird back down (right mouse button)",
            "R      reset the level",
            "Tab    pause or resume",
            "N      next level, once solved",
            "C      show contact points",
            "Esc    close this, or back to the menu",
        };
        const float h = 110.0f + 30.0f * static_cast<float>(lines.size() + std::size(keys));
        const SDL_FRect r{cx - 500.0f, 110.0f, 1000.0f, h};
        draw_panel(ren, r, color::kToolEdge);
        text_centered(ren, cx, r.y + 20.0f, 3.0f, color::kText, "HOW TO PLAY");
        float y = r.y + 66.0f;
        for (const std::string& l : lines) {
            text(ren, r.x + 40.0f, y, 2.0f, color::kText, "%s", l.c_str());
            y += 30.0f;
        }
        y += 20.0f;
        for (const char* l : keys) {
            text(ren, r.x + 40.0f, y, 2.0f, color::kDim, "%s", l);
            y += 30.0f;
        }
    } else if (level.status == Status::Won && app.slow <= 0) {
        const SDL_FRect r{cx - 360.0f, 140.0f, 720.0f, 170.0f};
        draw_panel(ren, r, color::kGood);
        text_centered(ren, cx, r.y + 14.0f, 4.0f, color::kGood, "SOLVED!");
        draw_stars(ren, cx, r.y + 78.0f, 16.0f, level.stars());
        const std::string line = level.result() + ", try " + std::to_string(level.attempt);
        text_centered(ren, cx, r.y + 106.0f, 2.0f, color::kText, line.c_str());
        const bool last = app.index + 1 >= level_count();
        text_centered(ren, cx, r.y + 138.0f, 2.0f, color::kDim, last ? "N menu   R play again" : "N next level   R play again");
    } else if (level.status == Status::Failed) {
        const SDL_FRect r{cx - 360.0f, 140.0f, 720.0f, 112.0f};
        draw_panel(ren, r, color::kBad);
        text_centered(ren, cx, r.y + 14.0f, 3.0f, color::kBad, "NOT SOLVED");
        text_centered(ren, cx, r.y + 50.0f, 2.0f, color::kText, hint.c_str());
        text_centered(ren, cx, r.y + 80.0f, 2.0f, color::kDim, "R try again   Esc menu");
    }
}

void draw_menu(SDL_Renderer* ren, const App& app) {
    draw_grid(ren);
    const float cx = static_cast<float>(kWidth) * 0.5f;
    text_centered(ren, cx, 60.0f, 6.0f, color::kText, "SLING");
    text_centered(ren, cx, 126.0f, 2.0f, color::kDim, "Pull back, aim, let go. Pop every pig.");
    char total[48];
    SDL_snprintf(total, sizeof(total), "%d of %d stars", app.progress.total_stars(), 3 * level_count());
    text_centered(ren, cx, 172.0f, 2.0f, color::kTarget, total);
    for (int i = 0; i < level_count(); ++i) {
        const SDL_FRect r = card_rect(i);
        const bool selected = i == app.index;
        const std::string name = make_layout(i).name;
        const Best best = app.progress.best(name);
        fill_rect(ren, r, selected ? SDL_Color{48, 56, 78, 255} : SDL_Color{33, 38, 52, 255});
        outline_rect(ren, r, selected ? color::kToolEdge : best.stars > 0 ? color::kDoneEdge : color::kPanelEdge);
        text(ren, r.x + 16.0f, r.y + 14.0f, 3.0f, color::kTarget, "%d", i + 1);
        text(ren, r.x + 16.0f, r.y + 52.0f, 2.0f, color::kText, "%s", name.c_str());
        draw_stars(ren, r.x + r.w - 52.0f, r.y + 28.0f, 9.0f, best.stars);
        if (best.stars > 0) text(ren, r.x + 16.0f, r.y + 82.0f, 2.0f, color::kDim, "best %d", best.score);
    }
    text_centered(ren, cx, 640.0f, 2.0f, color::kDim, "Click a level, or arrows and Enter.   Q quit");
}

void draw(SDL_Renderer* ren, const App& app) {
    set_color(ren, color::kBackground);
    SDL_RenderClear(ren);
    if (!app.level) {
        draw_menu(ren, app);
        return;
    }
    draw_level(ren, *app.level, app.paused || app.show_help ? std::nullopt : app.pull, app.show_contacts, &app.fx);
    app.fx.draw_over(ren, *app.level, to_world(0, kBarHeight).y);
    draw_hud(ren, app);
}

// ---- scripted run (see the top of the file) ----

bool run_script(App& app, SDL_Renderer* ren, int argc, char** argv) {
    auto number = [&](int i) { return i < argc ? static_cast<float>(std::atof(argv[i])) : 0.0f; };
    auto is = [](const char* a, const char* b) { return std::strcmp(a, b) == 0; };
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (is(a, "--level")) {
            start_level(app, static_cast<int>(number(++i)) - 1);
        } else if (is(a, "--mouse") || is(a, "--press") || is(a, "--drag") || is(a, "--release") || is(a, "--click")) {
            const SDL_FPoint p = to_screen({number(i + 1), number(i + 2)});
            i += 2;
            if (is(a, "--press") || is(a, "--click")) press(app, p.x, p.y);
            if (is(a, "--mouse") || is(a, "--drag")) move(app, p.x, p.y);
            if (is(a, "--release") || is(a, "--click")) release(app, p.x, p.y);
        } else if (is(a, "--shoot")) {
            const float deg = number(i + 1), speed = number(i + 2);
            i += 2;
            if (app.level) app.level->fire(app.level->pull_for(deg * phys::kPi / 180.0f, speed));
        } else if (is(a, "--steps")) {
            const int n = static_cast<int>(number(++i));
            for (int s = 0; s < n; ++s) update(app, kTimeStep * 1.0001f);
        } else if (is(a, "--key") && i + 1 < argc) {
            const char* k = argv[++i];
            key(app, is(k, "reset") ? SDLK_R : is(k, "pause") ? SDLK_TAB : is(k, "help") ? SDLK_H : is(k, "contacts") ? SDLK_C
                     : is(k, "next") ? SDLK_N : SDLK_ESCAPE);
        } else if (is(a, "--capture") && i + 1 < argc) {
            draw(ren, app);
            SDL_Surface* shot = SDL_RenderReadPixels(ren, nullptr);
            const bool ok = shot && SDL_SaveBMP(shot, argv[++i]);
            SDL_DestroySurface(shot);
            if (!ok) {
                SDL_Log("capture failed: %s", SDL_GetError());
                return false;
            }
        } else {
            SDL_Log("unknown argument: %s", a);
            return false;
        }
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Sling", kWidth, kHeight, 0, &window, &renderer)) {
        SDL_Log("window creation failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    App app;
    int result = 0;
    if (argc > 1) {
        app.scripted = true;
        result = run_script(app, renderer, argc, argv) ? 0 : 1;
    } else {
        load_progress(app);
        Uint64 last = SDL_GetTicksNS();
        while (app.running) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) handle_event(app, e);
            const Uint64 now = SDL_GetTicksNS();
            update(app, static_cast<phys::Real>(static_cast<double>(now - last) * 1e-9));
            last = now;
            draw(renderer, app);
            SDL_RenderPresent(renderer);
        }
    }

    app.level.reset();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
