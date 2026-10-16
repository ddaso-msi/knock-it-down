#pragma once
// One recorded solution per level, at par, found by tools/kid_solve and pasted here. A shot is the
// fixed step it was fired on (counted from the level's start), the sling's pull, and the step its power
// was used on (-1: never; a bomber then went off on impact). Replaying these shots step for step has to win: if a
// change to the engine, the rules or a layout breaks one, the level needs looking at again.
#include <phys/math.hpp>

#include <vector>

namespace kid::testdata {

struct Shot {
    long step;
    phys::Vec2 pull;
    long power;
};

struct Solution {
    const char* level;
    std::vector<Shot> shots;
};

inline const std::vector<Solution>& solutions() {
    static const std::vector<Solution> s = {
        {"First Pig", {{0, {-1.55619991f, -0.1088202f}, -1}}},
        {"Glass House", {{0, {-0.491669655f, 0.169295311f}, -1}}},
        {"Wood Fort", {{0, {-0.596184731f, -1.2223599f}, -1}}},
        {"Stone Wall", {{0, {-1.23320723f, -0.129615307f}, -1}}},
        {"Two Towers", {{0, {-1.47500885f, 0.50788641f}, -1}, {383, {-0.491669655f, 0.169295311f}, -1}}},
        {"Bunker", {{0, {-0.617076159f, -0.735402584f}, -1}, {254, {-1.55904961f, -0.054443121f}, -1}}},
        {"Ice Palace", {{0, {-1.55999994f, 0.0f}, 48}}},
        {"Timber Yard", {{0, {-0.761028409f, -0.875463128f}, 72}, {396, {-1.13462234f, 0.390681744f}, -1}}},
        {"Rooftops", {{0, {-1.11145341f, -1.15094364f}, 96}, {793, {-1.28590524f, 0.442772627f}, -1}}},
        {"Mixed Flock", {{0, {-1.22473359f, -0.193978786f}, -1}, {357, {-0.491669655f, 0.169295311f}, -1}}},
    };
    return s;
}

}  // namespace kid::testdata
