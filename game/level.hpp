#pragma once
// The rules. A Level builds a World from a Layout and plays it: the player fires shots from the launcher
// into a running simulation, and the level decides when every target is off its platform (a win),
// when a keep has been lost or the shots have run out with targets still up (a failure).
//
// A Level is plain data and can be copied mid-run: the copy carries on exactly as the original would.
// The solver (tools/solve.cpp) relies on that to branch a game at every shot. Nothing here knows about
// windows or input devices.
#include "layout.hpp"

#include <optional>
#include <string>
#include <vector>

namespace kid {

enum class Status { Playing, Won, Failed };
enum class Failure { None, OutOfShots, KeepLost };

// What a body (or a mark on the screen) means to the player. The app picks colours from this.
enum class Role { Scenery, Driven, Target, Done, Keep, Lost, Loose, Ball, Heavy, Blast, Hidden, Guide };

// Marks the level wants drawn on top of the world, in world coordinates.
struct Overlay {
    struct Line { Vec2 a, b; Role role; bool dashed; };
    struct Dot { Vec2 p; Real radius; Role role; };
    struct Ring { Vec2 p; Real radius; float fade; };  // fade: 1 just happened, 0 gone
    struct Label { Vec2 p; std::string text; Role role; };
    std::vector<Line> lines;
    std::vector<Dot> dots;
    std::vector<Ring> rings;
    std::vector<Label> labels;
};

class Level {
public:
    // Launch speed grows with how far the aim point is from the muzzle, between these limits. The top
    // speed moves a ball 17 cm a step, well under the thinnest piece plus a ball: the engine's continuous
    // collision only covers static bodies, so a much faster shot could pass through a block.
    static constexpr Real kMinSpeed = 4.0f, kMaxSpeed = 20.0f, kSpeedPerMeter = 2.2f;
    static constexpr Real kReloadTime = 0.4f;  // simulated seconds between shots: the last one is clear by then
    // After the last shot: once nothing has moved for kQuietTime the result is in; kSettleTime at most.
    static constexpr Real kQuietTime = 1.0f, kSettleTime = 8.0f;
    // With keeps in the level, "every target down" has to hold this long before it counts, so a keep
    // that is still on its way off the platform is not missed.
    static constexpr Real kWinHold = 1.0f;
    static constexpr Real kBlastRadius = 2.5f, kBlastImpulse = 8.0f;  // N s at the centre, none at the edge

    World world;
    Status status = Status::Playing;
    Failure failure = Failure::None;
    double sim_time = 0;  // simulated seconds in the current attempt
    double win_time = 0;  // sim_time at which the level was solved
    int attempt = 1;      // counts attempts in which a shot was fired; survives reset()

    std::vector<int> targets, keeps;  // indices into world.bodies
    std::vector<int> shots;           // the body each shot became, in firing order
    int fallen = 0;                   // targets below their fall line after the latest step
    int live_blast = -1;              // a blast shot still in flight, or -1

    explicit Level(Layout layout);

    const Layout& layout() const { return layout_; }
    const std::string& name() const { return layout_.name; }

    // The level exactly as it first appeared.
    void reset();
    // One fixed step of physics, then the rules.
    void step();

    // The velocity a shot would get if the player aimed at world point `aim`.
    Vec2 launch_velocity(Vec2 aim) const;
    // The aim point that gives a shot this direction (radians) and speed (clamped to the limits).
    Vec2 aim_for(Real angle, Real speed) const;
    bool can_fire() const;
    // Fires the next shot at `aim` if allowed; returns whether one was launched.
    bool fire(Vec2 aim);
    // Sets off the blast shot in flight, if there is one.
    bool detonate();
    // What a click does: set off a blast in flight, else fire.
    bool press(Vec2 aim) { return detonate() || fire(aim); }

    int shots_used() const { return static_cast<int>(shots.size()); }
    int shots_left() const { return static_cast<int>(layout_.ammo.size()) - shots_used(); }
    std::optional<Ammo> next_ammo() const;
    // 3 for par or better, 2 for one over, 1 for any other win; 0 unless won.
    int stars() const;
    // Nothing that matters is moving (driven parts like a windmill do not count).
    bool quiet() const;
    double since_last_shot() const { return sim_time - last_shot_time_; }

    Role role(size_t body) const;
    Material material(size_t body) const;
    void overlay(Overlay& out, Vec2 pointer, bool pointer_active) const;
    std::vector<std::string> help() const;  // how to play, a line each
    std::string hint() const;               // what to do right now
    std::string stats() const;              // counters for the status bar
    std::string result() const;             // the score line shown on a win

private:
    struct Drive {  // a slider's motor, steered every step along the swing
        int joint;
        int body;
        Vec2 origin;
        Slider slider;
    };
    struct Flash {
        Vec2 p;
        double time;
    };

    void build();
    void evaluate();
    void explode(int body);
    bool is_down(int body) const;

    Layout layout_;
    std::vector<int> piece_of_;  // for each body, its index in layout_.pieces, or -1
    std::vector<int> watched_;   // the bodies quiet() looks at
    std::vector<int> driven_;    // bodies a motor keeps moving
    std::vector<Drive> drives_;
    std::vector<Flash> flashes_;
    std::vector<Ammo> shot_ammo_;  // per shot
    double last_shot_time_ = 0;
    double quiet_for_ = 0;
    double win_hold_ = 0;
    bool tried_ = false;
};

}  // namespace kid
