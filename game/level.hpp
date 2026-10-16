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
enum class Role { Scenery, Driven, Block, Pig, Ball, Heavy, Blast, Split, Dart, Dropper, Egg, Hidden, Guide, Band };
Role role_of(Ammo a);  // how a bird of this kind is drawn

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
    // Powers. A dart dashes at kDashSpeed: 25 cm a step, still short of what a bird would need to get
    // through the thinnest post in one step (its own radius plus the post's half width, 34 cm), since
    // the engine's continuous collision does not cover moving bodies. A triple splits into three
    // fanned kSplitAngle apart. A dropper's egg falls at kEggSpeed and is a small blast; the dropper
    // itself leaves at kHopSpeed, up and away at 50 degrees (slow enough to stay on screen).
    static constexpr Real kDashSpeed = 30.0f, kSplitAngle = 0.2f, kEggSpeed = 8.0f, kHopSpeed = 13.0f;
    static constexpr Real kEggRadius = 0.2f, kEggBlastRadius = 1.6f, kEggImpulse = 5.0f, kEggDamage = 5.0f;
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
    std::vector<int> shots;         // the body each shot became, in firing order (a triple's other two
                                    // birds and a dropper's egg are extra bodies of the same shot)
    int pigs_left = 0;

    // Every pop this attempt, in order, for whoever draws the game (debris, the points floating up).
    struct Pop {
        Vec2 pos;
        int points;
        Kind kind;
        Material material;
        Real size;    // roughly how big it was, in metres
        double time;  // sim_time
    };
    std::vector<Pop> pops;

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
    // The latest bird's power can be used: it has one, it has not been used, and the bird has not yet
    // touched anything.
    bool power_ready() const;
    // Uses it (explode, split, dash, drop an egg). Returns whether anything happened. This is what a
    // click does while a bird is in flight.
    bool power();
    // Something is waiting to explode (a bomber in flight, an egg on its way down).
    bool blast_live() const { return !fuses_.empty(); }
    // Which shot a body came from (index into `shots`), or -1 for anything that is not a bird or an egg.
    int shot_of(size_t body) const { return body < shot_of_.size() ? shot_of_[body] : -1; }

    int shots_used() const { return static_cast<int>(shots.size()); }
    int shots_left() const { return static_cast<int>(layout_.ammo.size()) - shots_used(); }
    std::optional<Ammo> next_ammo() const;
    bool popped(int body) const { return world.bodies[static_cast<size_t>(body)].mask == 0; }
    // By score against the layout's thresholds: 3, 2, or 1 for any other win; 0 unless won.
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
    struct Fuse {  // something that explodes at the first thing it touches
        int body;
        Real radius, impulse, damage;
    };

    void build();
    void apply_damage(const std::vector<Vec2>& vel, const std::vector<Real>& spin);
    void roll_drag();
    void hurt(int body, Real amount);
    void pop(int body);
    void remove(int body);
    void evaluate();
    void explode(const Fuse& f);
    int add_bird(int shot, Ammo kind, Real radius, Vec2 pos, Vec2 vel);

    Layout layout_;
    std::vector<int> piece_of_;  // for each body, its index in layout_.pieces, or -1
    std::vector<Real> max_health_;
    std::vector<int> watched_;   // the bodies quiet() looks at
    std::vector<int> driven_;    // bodies a motor keeps moving
    std::vector<Drive> drives_;
    std::vector<Flash> flashes_;
    std::vector<Ammo> shot_ammo_;  // per shot
    std::vector<int> shot_of_;     // per body: see shot_of()
    std::vector<Fuse> fuses_;
    int flying_ = -1;              // the latest bird while its power is still ready, else -1
    double last_shot_time_ = 0;
    double quiet_for_ = 0;
    bool tried_ = false;
};

}  // namespace kid
