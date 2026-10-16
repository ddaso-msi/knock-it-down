#include "levels/levels.hpp"

namespace kid {

namespace {

using enum Material;
using enum Ammo;
using enum Pig;

constexpr Real kGround = kFloorTop;

// 1. One pig on a wooden table. Knock the table over, or hit the pig.
Layout first_pig() {
    Layout l;
    l.name = "First Pig";
    l.objective = "Pop the pig.";
    l.ammo = {Ball, Ball, Ball};
    l.par = 1;
    const Real top = frame(l, Wood, 14.0f, kGround, 1.2f, 1.2f);
    pig(l, Small, 14.0f, top);
    return l;
}

// 2. Ice: brittle. The whole house comes down on the pig inside it.
Layout glass_house() {
    Layout l;
    l.name = "Glass House";
    l.objective = "Pop both pigs. Ice breaks easily.";
    l.notes = {"Pale blocks are ice: they shatter at a touch, and they slide."};
    l.ammo = {Ball, Ball, Ball};
    l.par = 1;
    pig(l, Small, 14.0f, kGround);
    const Real top = frame(l, Ice, 14.0f, kGround, 1.6f, 1.2f);
    const Real roof = frame(l, Ice, 14.0f, top, 1.0f, 0.9f);
    pig(l, Small, 14.0f, roof);
    return l;
}

// 3. A two-storey wooden fort with a pig on every floor.
Layout wood_fort() {
    Layout l;
    l.name = "Wood Fort";
    l.objective = "Pop all three pigs.";
    l.ammo = {Ball, Ball, Ball};
    l.par = 1;
    pig(l, Small, 14.5f, kGround);
    const Real first = frame(l, Wood, 14.5f, kGround, 2.0f, 1.2f);
    pig(l, Small, 14.5f, first);
    const Real second = frame(l, Wood, 14.5f, first, 1.2f, 1.0f);
    pig(l, Small, 14.5f, second);
    column(l, Wood, 16.6f, kGround, 3, {0.3f, 0.3f});
    return l;
}

// 4. A stone wall in front of a helmet pig. A ball bounces off stone; the heavy bird does not.
Layout stone_wall() {
    Layout l;
    l.name = "Stone Wall";
    l.objective = "Get through the stone to the pigs.";
    l.notes = {"Grey blocks are stone. The big dark bird is five times heavier.",
               "The helmet pig takes a few hits to pop."};
    l.ammo = {Heavy, Ball, Ball};
    l.par = 1;
    pyramid(l, Stone, 12.0f, kGround, 3, 0.35f);
    pig(l, Helmet, 14.2f, kGround);
    const Real top = frame(l, Stone, 14.2f, kGround, 1.4f, 1.2f);  // a stone roof: no lobbing in
    pig(l, Small, 14.2f, top);
    return l;
}

// 5. Two towers, a pig on each, and a big one between them.
Layout two_towers() {
    Layout l;
    l.name = "Two Towers";
    l.objective = "Pop all three pigs.";
    l.notes = {"The big pig is tough: drop a tower on it."};
    l.ammo = {Ball, Ball, Ball, Ball};
    l.par = 2;
    Real a = frame(l, Wood, 12.0f, kGround, 1.0f, 1.4f);
    a = frame(l, Ice, 12.0f, a, 1.0f, 1.2f);
    pig(l, Small, 12.0f, a);
    pig(l, Big, 14.25f, kGround);
    Real b = frame(l, Wood, 16.5f, kGround, 1.0f, 1.4f);
    b = frame(l, Wood, 16.5f, b, 1.0f, 1.2f);
    b = frame(l, Wood, 16.5f, b, 1.0f, 1.0f);  // not ice: a pig rolls off a plank that slippery
    pig(l, Small, 16.5f, b);
    return l;
}

// 6. A big pig in a concrete bunker on a ledge. Nothing breaks concrete; a blast reaches through it.
Layout bunker() {
    Layout l;
    l.name = "Bunker";
    l.objective = "Get the big pig in the bunker.";
    l.notes = {"Red birds explode when they hit something (or when you click in flight).",
               "A blast pushes and damages everything within 2.5 m, even through walls."};
    l.ammo = {Blast, Blast, Ball};
    l.par = 2;
    platform(l, 15.0f, 2.0f, 2.0f);
    wall(l, {13.6f, 2.7f}, {0.2f, 0.7f});  // the bunker: concrete, part of the scenery
    wall(l, {16.4f, 2.7f}, {0.2f, 0.7f});
    wall(l, {15.0f, 3.55f}, {1.6f, 0.15f});
    pig(l, Big, 15.0f, 2.0f);
    pig(l, Small, 15.0f, 3.7f);
    return l;
}

// 7. A row of ice rooms. The triple splits into three, and each one is murder on ice.
Layout ice_palace() {
    Layout l;
    l.name = "Ice Palace";
    l.objective = "Pop every pig in the palace.";
    l.notes = {"Teal birds are triples: click in flight and they split in three.",
               "Triples are three times as hard on ice, and soft on wood and stone."};
    l.ammo = {Split, Split, Ball};
    l.par = 1;
    for (const Real x : {12.0f, 13.9f, 15.8f}) {
        pig(l, Small, x, kGround);
        const Real top = frame(l, Ice, x, kGround, 1.2f, 1.1f);
        if (x != 13.9f) pig(l, Small, x, top);
    }
    return l;
}

// 8. Pigs behind a thick wooden wall. A dart dashes, and wood is no match for it.
Layout timber_yard() {
    Layout l;
    l.name = "Timber Yard";
    l.objective = "Break through the timber wall.";
    l.notes = {"Gold birds are darts: click in flight and they dash forward, fast.",
               "Darts are three times as hard on wood, and soft on stone."};
    l.ammo = {Dart, Dart, Ball};
    l.par = 2;
    for (const Real x : {11.4f, 12.41f}) column(l, Wood, x, kGround, 3, {0.5f, 0.5f});  // big, tough blocks
    pig(l, Small, 14.0f, kGround);
    pig(l, Helmet, 15.4f, kGround);
    const Real top = frame(l, Wood, 14.7f, kGround, 2.4f, 1.0f);
    pig(l, Small, 14.7f, top);
    return l;
}

// 9. Pigs in stone huts. Nothing thrown at them does much; an egg dropped on the roof does.
Layout rooftops() {
    Layout l;
    l.name = "Rooftops";
    l.objective = "Pop the pigs in the stone huts.";
    l.notes = {"White birds are droppers: click in flight to drop an egg straight down.",
               "The egg explodes where it lands, through the roof below it."};
    l.ammo = {Dropper, Dropper, Dropper};
    l.par = 2;
    for (const Real x : {11.0f, 14.0f, 17.0f}) {
        pig(l, Small, x, kGround);
        frame(l, Stone, x, kGround, 1.0f, 0.9f);
    }
    return l;
}

// 10. Stone, wood and ice in one fort, and one bird for each.
Layout mixed_flock() {
    Layout l;
    l.name = "Mixed Flock";
    l.objective = "Pop all four pigs.";
    l.notes = {"Each bird is best at something: heavy on stone, dart on wood, triple on ice."};
    l.ammo = {Heavy, Dart, Split, Blast};
    l.par = 2;
    pig(l, Big, 14.5f, kGround);
    Real y = frame(l, Stone, 14.5f, kGround, 1.6f, 1.2f);
    pig(l, Helmet, 14.5f, y);
    y = frame(l, Wood, 14.5f, y, 1.2f, 1.0f);
    pig(l, Small, 14.5f, y);
    Real ice = frame(l, Ice, 17.3f, kGround, 1.0f, 1.3f);
    ice = frame(l, Ice, 17.3f, ice, 1.0f, 1.1f);
    pig(l, Small, 17.3f, ice);
    return l;
}

using Maker = Layout (*)();
constexpr Maker kLevels[] = {first_pig,  glass_house, wood_fort, stone_wall, two_towers,
                             bunker,     ice_palace,  timber_yard, rooftops, mixed_flock};

}  // namespace

int level_count() { return static_cast<int>(std::size(kLevels)); }

Layout make_layout(int index) {
    if (index < 0 || index >= level_count()) return {};
    return kLevels[index]();
}

}  // namespace kid
