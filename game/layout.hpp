#pragma once
// A level as plain data: the arena's static walls, the loose pieces, what holds some of them in place,
// the launcher and what it fires. The rules (game/level.hpp) turn a Layout into a World and play it.
// Levels are written with the builder functions at the bottom (game/levels/levels.cpp).
#include <phys/world.hpp>

#include <string>
#include <vector>

namespace kid {

using phys::Body;
using phys::Real;
using phys::Vec2;
using phys::World;

// The arena, in metres. The app shows exactly this area, so levels are laid out in world units.
constexpr Real kWorldW = 20.0f, kWorldH = 11.25f;
constexpr Real kFloorTop = 0.5f;
// Every level advances by this step and nothing else, so a replay of the same shots is identical.
constexpr Real kTimeStep = 1.0f / 120.0f;

// What a loose piece is made of. Stone is three times as heavy as wood; ice is wood that slides.
enum class Material { Wood, Stone, Ice };
struct MaterialProps {
    Real density, friction, restitution;
};
MaterialProps material_props(Material m);

// What the launcher fires.
//   Ball   the ordinary shot
//   Heavy  bigger and five times the mass: it shoves stone where a ball bounces off
//   Blast  goes off at the first thing it touches (or when clicked in flight), pushing everything near it away
enum class Ammo { Ball, Heavy, Blast };
struct AmmoProps {
    const char* name;
    Real radius, density, restitution, friction;
};
AmmoProps ammo_props(Ammo a);

// What a loose piece means to the player.
//   Target  knock it off its platform (its centre below `fall_y`)
//   Keep    must NOT end up below `fall_y`: losing one fails the level at once
//   Loose   no objective: planks, barrels, counterweights, things to use
enum class Kind { Target, Keep, Loose };

struct Piece {
    Kind kind = Kind::Target;
    Material material = Material::Wood;
    Vec2 pos;
    Vec2 half{0.35f, 0.35f};  // a box's half extents...
    Real radius = 0;          // ...or, if positive, a disc of this radius
    Real angle = 0;
    Real fall_y = 0;          // Target and Keep: the line that counts
};

struct Wall {  // a static box
    Vec2 pos, half;
    Real angle = 0;
};

// A piece pinned to the world at `anchor`, free to turn. With a motor it turns at `speed` rad/s
// (counter-clockwise positive) whatever hits it, up to `torque`.
struct Hinge {
    int piece;
    Vec2 anchor;
    bool motor = false;
    Real speed = 0, torque = 0;
};

// A piece hung from the world: a rigid rod from `anchor` to the point `attach` (both world points, as laid out).
struct Rope {
    int piece;
    Vec2 anchor, attach;
};

// A piece on a rail along `axis` (a unit vector). A motor swings it smoothly from where it was laid
// out to 2 * `amplitude` metres along the axis and back, once every `period` seconds:
//     offset = amplitude * (1 - cos(2 pi t / period))
// It starts at rest, at one end. Both halves of that matter to anything riding on it: a swing that
// started at full speed would jerk the deck out from under its blocks, and the most it accelerates is
// amplitude * (2 pi / period)^2, which has to stay well under friction times g. It cannot turn, and the
// motor is strong enough that a shot barely slows it.
struct Slider {
    int piece;
    Vec2 axis{1, 0};
    Real amplitude = 2, period = 8;
    Real force = 5000;
};

// A dashed line the player sees: "below here counts as off". One per platform.
struct FallLine {
    Real y, x0, x1;
};

struct Layout {
    std::string name;
    std::string objective;          // one line in the status bar
    std::vector<std::string> notes; // extra help lines: what is new in this level
    Vec2 muzzle{2.0f, 2.6f};        // where a shot starts; a pedestal is built under it
    std::vector<Ammo> ammo;         // fired in this order
    int par = 1;                    // the fewest shots known to solve it (three stars)

    std::vector<Wall> walls;
    std::vector<Piece> pieces;
    std::vector<Hinge> hinges;
    std::vector<Rope> ropes;
    std::vector<Slider> sliders;
    std::vector<FallLine> fall_lines;
};

// ---- building layouts ----

// Adds a pieceless static box.
void wall(Layout& l, Vec2 pos, Vec2 half, Real angle = 0);
// A platform: a slab whose top is at `top`, centred on x, with a pillar down to the floor (unless
// `pillar` is false: then it is a shelf that has to be held up by walls you add yourself). Adds the
// fall line 0.1 m under the top, and returns that line's height for pieces standing on it.
Real platform(Layout& l, Real x, Real top, Real half_width, bool pillar = true);
// Adds one piece and returns its index.
int piece(Layout& l, Kind kind, Material m, Vec2 pos, Vec2 half, Real fall_y, Real angle = 0);
int disc(Layout& l, Kind kind, Material m, Vec2 pos, Real radius, Real fall_y);
// Equal square blocks of half size `half`, in rows of rows, rows-1, ... 1, standing on a surface at
// height `base`, centred on x. Returns the first piece's index (they are consecutive).
int pyramid(Layout& l, Kind kind, Material m, Real x, Real base, int rows, Real half, Real fall_y);
// `count` boxes stacked straight up from `base`, centred on x.
int column(Layout& l, Kind kind, Material m, Real x, Real base, int count, Vec2 half, Real fall_y);

}  // namespace kid
