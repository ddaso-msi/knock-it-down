#pragma once
// A level as plain data: the arena's static walls, the structure (breakable blocks) and the pigs in it,
// what holds some pieces in place, the slingshot and the birds it fires. The rules (game/level.hpp)
// turn a Layout into a World and play it. Levels are written with the builder functions at the bottom
// (game/levels/levels.cpp).
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
// The room's walls and ceiling go up to here: the top speed (20 m/s) cannot reach it.
constexpr Real kRoomTop = 40.0f;
constexpr Real kFloorTop = 0.5f;
// Every level advances by this step and nothing else, so a replay of the same shots is identical.
constexpr Real kTimeStep = 1.0f / 120.0f;

// What a block is made of. Ice is light, slippery and brittle; stone is heavy and tough.
enum class Material { Wood, Stone, Ice };
struct MaterialProps {
    Real density, friction, restitution;
    Real toughness;  // health per square metre of block, in N s of impact (see Level::step)
    int points;      // score for breaking one
};
MaterialProps material_props(Material m);

// The pigs. Popping every one of them wins the level.
enum class Pig { Small, Helmet, Big };
struct PigProps {
    const char* name;
    Real radius, health;
    int points;
};
PigProps pig_props(Pig p);

// The birds. Each one but Basic and Heavy has a power, used by clicking while it is still in flight
// and has not yet touched anything (see Level::power).
//   Ball     "basic": no power
//   Heavy    five times the mass of a basic bird; extra hard on stone
//   Blast    "bomber": goes off at the first thing it touches, or when clicked, pushing and damaging
//            everything within 2.5 m, even through walls
//   Split    "triple": splits into three smaller birds fanned out; extra hard on ice
//   Dart     dashes forward at Level::kDashSpeed; extra hard on wood
//   Dropper  drops an egg straight down (a small blast) and shoots up and away
enum class Ammo { Ball, Heavy, Blast, Split, Dart, Dropper };
struct AmmoProps {
    const char* name;
    const char* power;  // what a click does, for the hint line; empty if nothing
    Real radius, density, restitution, friction;
    // Damage multipliers for what this bird hits (pigs always take damage as it comes).
    Real vs_wood = 1, vs_ice = 1, vs_stone = 1;
};
AmmoProps ammo_props(Ammo a);
bool has_power(Ammo a);
// How hard this bird is on what it hits: a block of the given material, or a pig when `pig` is set.
Real damage_factor(Ammo a, Material m, bool pig);

enum class Kind { Block, Pig };

struct Piece {
    Kind kind = Kind::Block;
    Material material = Material::Wood;  // Block
    Pig pig = Pig::Small;                // Pig
    Vec2 pos;
    Vec2 half{0.35f, 0.35f};  // a box's half extents...
    Real radius = 0;          // ...or, if positive, a disc of this radius (pigs are always discs)
    Real angle = 0;
    bool breakable = true;    // false: takes no damage (a windmill's bar, a rail's deck)
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

struct Layout {
    std::string name;
    std::string objective;          // one line in the status bar
    std::vector<std::string> notes; // extra help lines: what is new in this level
    Vec2 pouch{2.5f, 2.6f};         // where the slingshot holds a bird at rest
    std::vector<Ammo> ammo;         // fired in this order
    int par = 1;                    // the fewest birds known to solve it
    // Score for two and three stars (any win is one). Set from kid_solve: three for a typical win at par,
    // two for one bird's bonus less.
    struct { int two = 0, three = 0; } stars;

    std::vector<Wall> walls;
    std::vector<Piece> pieces;
    std::vector<Hinge> hinges;
    std::vector<Rope> ropes;
    std::vector<Slider> sliders;
};

// ---- building layouts ----
// Pieces are laid out a millimetre apart so nothing starts out overlapping; `base` is the height of
// the surface a piece stands on.

void wall(Layout& l, Vec2 pos, Vec2 half, Real angle = 0);
// A static slab whose top is at `top`, centred on x, on a pillar down to the floor.
void platform(Layout& l, Real x, Real top, Real half_width);
// Adds one block and returns its index.
int block(Layout& l, Material m, Vec2 pos, Vec2 half, Real angle = 0);
// A disc of this material and radius (a boulder, a wrecking ball). Returns its index.
int boulder(Layout& l, Material m, Vec2 pos, Real radius);
// A pig sitting on a surface at height `base`, centred on x. Returns its index.
int pig(Layout& l, Pig p, Real x, Real base);
// An upright post of half width `hw` and height `h` standing on `base`. Returns its index.
int post(Layout& l, Material m, Real x, Real base, Real h, Real hw = 0.12f);
// A plank of half width `hw`, `t` thick, lying on a surface at `base`. Returns its index.
int plank(Layout& l, Material m, Real x, Real base, Real hw, Real t = 0.2f);
// A frame: two posts `span` apart and a plank across them, overhanging each post a little. Returns
// the height of the plank's top.
Real frame(Layout& l, Material m, Real x, Real base, Real span, Real h);
// Square blocks of half size `half` in rows of rows, rows-1, ... 1. Returns the first block's index.
int pyramid(Layout& l, Material m, Real x, Real base, int rows, Real half);
// `count` boxes stacked straight up. Returns the first one's index.
int column(Layout& l, Material m, Real x, Real base, int count, Vec2 half);

}  // namespace kid
