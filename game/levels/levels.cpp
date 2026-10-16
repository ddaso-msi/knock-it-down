#include "levels/levels.hpp"

namespace kid {

namespace {

using enum Kind;
using enum Material;
using enum Ammo;

// 1. The pyramid from Physics Puzzle Lab: one platform, six wooden blocks.
Layout first_shot() {
    Layout l;
    l.name = "First Shot";
    l.objective = "Knock every block off the platform.";
    l.ammo = {Ball, Ball, Ball};
    l.par = 2;
    const Real line = platform(l, 14.5f, 3.5f, 1.3f);
    pyramid(l, Target, Wood, 14.5f, 3.5f, 3, 0.35f, line);
    return l;
}

// 2. Two platforms at different heights and distances: one shot cannot reach both.
Layout two_shelves() {
    Layout l;
    l.name = "Two Shelves";
    l.objective = "Clear both platforms.";
    l.ammo = {Ball, Ball, Ball};
    l.par = 2;
    const Real near = platform(l, 10.0f, 2.6f, 0.8f);
    column(l, Target, Wood, 10.0f, 2.6f, 3, {0.35f, 0.35f}, near);
    const Real far = platform(l, 16.0f, 5.2f, 1.0f);
    pyramid(l, Target, Wood, 16.0f, 5.2f, 2, 0.35f, far);
    return l;
}

// 3. A wall between the launcher and the blocks: the shot has to go up and over.
Layout over_the_wall() {
    Layout l;
    l.name = "Over the Wall";
    l.objective = "Lob a shot over the wall.";
    l.notes = {"Aim high and not too hard: the ball has to drop in behind the wall."};
    l.ammo = {Ball, Ball, Ball};
    l.par = 1;
    wall(l, {8.0f, 2.75f}, {0.3f, 2.25f});
    const Real line = platform(l, 12.0f, 2.5f, 0.3f);
    column(l, Target, Wood, 12.0f, 2.5f, 4, {0.35f, 0.35f}, line);
    return l;
}

// 4. The blocks stand on a plank bridge between two legs. Knock out a leg and it all comes down.
Layout weak_point() {
    Layout l;
    l.name = "Weak Point";
    l.objective = "Bring the blocks down off the bridge.";
    l.notes = {"Brown pieces are loose: they count for nothing, but they hold things up."};
    l.ammo = {Ball, Ball, Ball};
    l.par = 1;
    platform(l, 14.0f, 2.0f, 2.2f);
    l.fall_lines.clear();  // the line that counts is the bridge's, not the platform's
    piece(l, Loose, Wood, {12.6f, 2.0f + 0.8f}, {0.15f, 0.8f}, 0);
    piece(l, Loose, Wood, {15.4f, 2.0f + 0.8f}, {0.15f, 0.8f}, 0);
    const Real deck = 2.0f + 1.6f + 0.001f;
    piece(l, Loose, Wood, {14.0f, deck + 0.12f}, {1.6f, 0.12f}, 0);
    const Real line = 2.0f + 0.8f;
    l.fall_lines.push_back({line, 11.6f, 16.4f});
    pyramid(l, Target, Wood, 14.0f, deck + 0.241f, 3, 0.3f, line);
    return l;
}

// 5. Ice slides: a long shelf, and a nudge sends a block a long way.
Layout ice_shelf() {
    Layout l;
    l.name = "Ice Shelf";
    l.objective = "Slide the ice off the long shelf.";
    l.notes = {"Pale blocks are ice. They hardly grip anything, not even each other."};
    l.ammo = {Ball, Ball, Ball};
    l.par = 2;
    const Real line = platform(l, 13.0f, 3.0f, 3.0f);
    column(l, Target, Ice, 10.8f, 3.0f, 2, {0.35f, 0.35f}, line);
    column(l, Target, Ice, 13.0f, 3.0f, 2, {0.35f, 0.35f}, line);
    column(l, Target, Ice, 15.2f, 3.0f, 2, {0.35f, 0.35f}, line);
    return l;
}

// 6. Stone: three times as heavy as wood. A ball bounces off it; the heavy shot does not.
Layout heavy_going() {
    Layout l;
    l.name = "Heavy Going";
    l.objective = "Topple the stone column.";
    l.notes = {"Grey blocks are stone. The big dark shot is five times heavier than a ball."};
    l.ammo = {Heavy, Ball, Ball};
    l.par = 1;
    const Real line = platform(l, 14.0f, 2.6f, 0.9f);
    column(l, Target, Stone, 14.0f, 2.6f, 3, {0.7f, 0.35f}, line);
    return l;
}

// 7. Purple blocks have to stay where they are: knocking one off fails the level at once.
Layout hands_off() {
    Layout l;
    l.name = "Hands Off";
    l.objective = "Clear the far platform. Spare the purple.";
    l.notes = {"Purple blocks must stay on their platform. Lose one and the level is over."};
    l.ammo = {Ball, Ball, Ball};
    l.par = 2;
    const Real near = platform(l, 8.5f, 2.4f, 0.6f);
    column(l, Keep, Wood, 8.5f, 2.4f, 2, {0.35f, 0.35f}, near);
    const Real far = platform(l, 14.5f, 3.0f, 1.3f);
    pyramid(l, Target, Wood, 14.5f, 3.0f, 3, 0.35f, far);
    return l;
}

// 8. The blocks sit in a garage that no shot can get into. A row of dominoes can.
Layout domino_run() {
    Layout l;
    l.name = "Domino Run";
    l.objective = "Push the blocks out of the garage.";
    l.notes = {"The roof stops every shot. Start something that reaches the blocks for you."};
    l.ammo = {Ball, Ball};
    l.par = 1;
    const Real line = platform(l, 12.5f, 4.0f, 3.5f);
    for (int i = 0; i < 7; ++i) piece(l, Loose, Wood, {9.5f + 0.75f * static_cast<Real>(i), 4.0f + 0.55f}, {0.1f, 0.55f}, 0);
    wall(l, {15.4f, 5.65f}, {1.1f, 0.15f});
    column(l, Target, Wood, 15.4f, 4.0f, 2, {0.3f, 0.3f}, line);
    return l;
}

// 9. Behind a high wall and under a roof: only a blast reaches them.
Layout blast_wall() {
    Layout l;
    l.name = "Blast Wall";
    l.objective = "Blast the blocks off from the other side of the wall.";
    l.notes = {"Red shots explode when they hit something (or when you click in flight),",
               "pushing everything within 2.5 m away from the blast, even through walls."};
    l.ammo = {Blast, Blast};
    l.par = 1;
    wall(l, {11.0f, 3.75f}, {0.3f, 3.25f});
    wall(l, {12.6f, 4.75f}, {1.3f, 0.15f});  // roof
    const Real line = platform(l, 12.7f, 3.0f, 1.0f);
    pyramid(l, Target, Wood, 12.5f, 3.0f, 2, 0.35f, line);
    return l;
}

// 10. The platform will not hold still.
Layout moving_target() {
    Layout l;
    l.name = "Moving Target";
    l.objective = "Clear the moving platform.";
    l.notes = {"The platform glides back and forth. Lead the target."};
    l.ammo = {Ball, Ball, Ball};
    l.par = 1;
    const Vec2 at{11.0f, 4.6f}, half{1.1f, 0.15f};  // the left end of its swing
    constexpr Real kSwing = 2.5f;                       // it glides 5 m to the right and back
    const int deck = piece(l, Loose, Stone, at, half, 0);
    l.sliders.push_back({deck, {1, 0}, kSwing, 7.0f});
    const Real top = at.y + half.y, line = top - 0.1f;
    l.fall_lines.push_back({line, at.x - half.x - 0.6f, at.x + 2 * kSwing + half.x + 0.6f});
    pyramid(l, Target, Wood, at.x, top + 0.001f, 2, 0.32f, line);
    return l;
}

// 11. A stone wrecking ball hangs in front of the stack. The stack is out of reach; the ball is not.
Layout wrecking_ball() {
    Layout l;
    l.name = "Wrecking Ball";
    l.objective = "Swing the wrecking ball into the stack.";
    l.notes = {"The ball on the rope is stone. Hit it hard enough and it swings."};
    l.ammo = {Ball, Ball, Ball};
    l.par = 1;
    wall(l, {12.0f, 0.5f + 1.15f}, {0.25f, 1.15f});  // low wall under the wrecking ball
    // Roof over the stack. Its left end reaches over the wrecking ball's shoulder: any shorter and a
    // steep lob slips down between the two.
    wall(l, {14.4f, 4.4f}, {2.1f, 0.15f});
    const Real line = platform(l, 14.0f, 2.6f, 0.9f);
    for (int i = 0; i < 2; ++i) column(l, Target, Wood, 13.65f + 0.75f * static_cast<Real>(i), 2.6f, 2, {0.35f, 0.35f}, line);
    const int ball = disc(l, Loose, Stone, {12.0f, 3.4f}, 0.55f, 0);
    l.ropes.push_back({ball, {12.0f, 8.4f}, {12.0f, 3.4f}});
    return l;
}

// 12. A turning bar guards the only way in. Time the shot.
Layout windmill() {
    Layout l;
    l.name = "Windmill";
    l.objective = "Get a shot past the windmill.";
    l.notes = {"The bar turns all the time and nothing stops it. Fire when the way is clear."};
    l.ammo = {Ball, Ball, Ball};
    l.par = 2;
    wall(l, {15.0f, 5.9f}, {2.2f, 0.15f});  // roof: no lobbing in from above
    const Real line = platform(l, 15.0f, 3.0f, 1.3f);
    pyramid(l, Target, Wood, 15.0f, 3.0f, 3, 0.35f, line);
    const int bar = piece(l, Loose, Wood, {11.0f, 4.4f}, {1.7f, 0.12f}, 0);
    l.hinges.push_back({bar, {11.0f, 4.4f}, true, 1.4f, 3000.0f});
    wall(l, {11.0f, 0.5f + 1.0f}, {0.2f, 1.0f});  // the post: clear of the bar's tips, but no shot fits under them
    return l;
}

using Maker = Layout (*)();
constexpr Maker kLevels[] = {first_shot, two_shelves, over_the_wall, weak_point,    ice_shelf,     heavy_going,
                             hands_off,  domino_run,  blast_wall,    moving_target, wrecking_ball, windmill};

}  // namespace

int level_count() { return static_cast<int>(std::size(kLevels)); }

Layout make_layout(int index) {
    if (index < 0 || index >= level_count()) return {};
    return kLevels[index]();
}

}  // namespace kid
