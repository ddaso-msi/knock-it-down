#pragma once
// The rules. A Level builds a World from a Layout and plays it: the player pulls a bird back in the
// slingshot and lets go, into a running simulation. Blocks and pigs take damage from hard impacts and
// pop when their health runs out. Popping every pig wins; running out of birds with pigs left fails.
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

// What a body (or a mark on the screen) means to the player. The app picks colours from this.
enum class Role { Scenery, Driven, Block, Pig, Ball, Heavy, Blast, Hidden, Guide, Band };

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
    // The slingshot. A pull is the vector from the pouch to where the bird is held; letting go fires
    // the bird the opposite way at kPullSpeed per metre of pull. Pulls shorter than kMinPull are not a
    // shot, and pulls are cut to kMaxPull, which gives the top speed: 20 m/s moves a bird 17 cm a step,
    // well under the thinnest piece plus a bird (the engine's continuous collision only covers static
    // bodies, so a much faster shot could pass through a block).
    static constexpr Real kMaxPull = 1.6f, kMinPull = 0.25f, kPullSpeed = 12.5f;
    static constexpr Real kReloadTime = 0.4f;  // simulated seconds between shots: the last one is clear by then
    // After the last shot: once nothing has moved for kQuietTime the result is in; kSettleTime at most.
    static constexpr Real kQuietTime = 1.0f, kSettleTime = 8.0f;
    static constexpr Real kBlastRadius = 2.5f, kBlastImpulse = 8.0f;  // N s at the centre, none at the edge
    static constexpr Real kBlastDamage = 6.0f;  // health a blast takes from what is at its centre
    // An impact only hurts when the two surfaces were closing faster than this: resting weight and
    // gentle nudges cost nothing, however heavy the stack.
    static constexpr Real kDamageSpeed = 1.0f;
    static constexpr int kBirdBonus = 10000;  // score for each bird left over on a win
    // Rolling resistance for round things (pigs, birds) while they touch something, in rad/s^2. The
    // engine has none, so a disc on a plank tilted by a tenth of a degree rolls off in time. This stops
    // a pig on anything flatter than about 5 degrees, and lets a bird come to rest on the ground.
    static constexpr Real kRollDrag = 2.0f;

    World world;
    Status status = Status::Playing;
    double sim_time = 0;  // simulated seconds in the current attempt
    double win_time = 0;  // sim_time at which the level was solved
    int attempt = 1;      // counts attempts in which a shot was fired; survives reset()
    int score = 0;

    std::vector<int> pigs, blocks;  // indices into world.bodies (popped ones stay listed)
    std::vector<Real> health;       // per body; only meaningful for pigs and breakable blocks
    std::vector<int> shots;         // the body each shot became, in firing order
    int pigs_left = 0;
    int live_blast = -1;            // a blast shot still in flight, or -1

    explicit Level(Layout layout);

    const Layout& layout() const { return layout_; }
    const std::string& name() const { return layout_.name; }

    // The level exactly as it first appeared.
    void reset();
    // One fixed step of physics, then damage, then the rules.
    void step();

    // The pull you get by holding the bird at `pointer`: clamped in length, and kept off the floor.
    Vec2 pull_to(Vec2 pointer) const;
    // Where the bird sits for a given pull, and the velocity it leaves with.
    Vec2 launch_point(Vec2 pull) const { return layout_.pouch + pull; }
    Vec2 launch_velocity(Vec2 pull) const { return pull * -kPullSpeed; }
    // The pull that fires in this direction (radians) at this speed (clamped to what the sling allows).
    Vec2 pull_for(Real angle, Real speed) const;
    bool can_fire() const;
    // Fires the next bird with this pull if allowed (and long enough); returns whether one was launched.
    bool fire(Vec2 pull);
    // Sets off the blast shot in flight, if there is one.
    bool detonate();

    int shots_used() const { return static_cast<int>(shots.size()); }
    int shots_left() const { return static_cast<int>(layout_.ammo.size()) - shots_used(); }
    std::optional<Ammo> next_ammo() const;
    bool popped(int body) const { return world.bodies[static_cast<size_t>(body)].mask == 0; }
    // 3 for par or better, 2 for one over, 1 for any other win; 0 unless won.
    int stars() const;
    // Nothing that matters is moving (driven parts like a windmill do not count).
    bool quiet() const;
    double since_last_shot() const { return sim_time - last_shot_time_; }

    Role role(size_t body) const;
    Material material(size_t body) const;
    // 1 untouched, 0 about to pop. Bodies that cannot be damaged are always 1.
    Real condition(size_t body) const;
    std::optional<Pig> pig_kind(size_t body) const;
    // `pull`: the bird is being held back in the sling right now.
    void overlay(Overlay& out, std::optional<Vec2> pull) const;
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
        Real radius;
    };

    void build();
    void apply_damage(const std::vector<Vec2>& vel, const std::vector<Real>& spin);
    void roll_drag();
    void hurt(int body, Real amount);
    void pop(int body);
    void remove(int body);
    void evaluate();
    void explode(int body);

    Layout layout_;
    std::vector<int> piece_of_;  // for each body, its index in layout_.pieces, or -1
    std::vector<Real> max_health_;
    std::vector<int> watched_;   // the bodies quiet() looks at
    std::vector<int> driven_;    // bodies a motor keeps moving
    std::vector<Drive> drives_;
    std::vector<Flash> flashes_;
    std::vector<Ammo> shot_ammo_;  // per shot
    double last_shot_time_ = 0;
    double quiet_for_ = 0;
    bool tried_ = false;
};

}  // namespace kid
