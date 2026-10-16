#pragma once
// One recorded solution per level, at par, found by tools/kid_solve and pasted here. A shot is the
// fixed step it was fired on (counted from the level's start), the aim point, and for a blast the step
// it was set off on (-1: it went off on impact). Replaying these shots step for step has to win: if a
// change to the engine, the rules or a layout breaks one, the level needs looking at again.
#include <phys/math.hpp>

#include <vector>

namespace kid::testdata {

struct Shot {
    long step;
    phys::Vec2 aim;
    long detonate;
};

struct Solution {
    const char* level;
    std::vector<Shot> shots;
};

inline const std::vector<Solution>& solutions() {
    static const std::vector<Solution> s = {
        {"First Shot", {{0, {9.04769516f, 5.16515112f}, -1}, {595, {10.3018045f, 4.98050404f}, -1}}},
        {"Two Shelves", {{0, {10.2749166f, 5.776443f}, -1}, {580, {10.7773752f, 3.83357954f}, -1}}},
        {"Over the Wall", {{0, {5.47329187f, 6.19669914f}, -1}}},
        {"Weak Point", {{0, {10.4763374f, 5.19147635f}, -1}}},
        {"Ice Shelf", {{0, {10.6003485f, 4.74430752f}, -1}, {1348, {10.6699448f, 4.44285345f}, -1}}},
        {"Heavy Going", {{0, {9.93878365f, 4.57936096f}, -1}}},
        {"Hands Off", {{0, {10.6364613f, 4.59388447f}, -1}, {749, {10.4763374f, 5.19147635f}, -1}}},
        {"Domino Run", {{0, {10.1155262f, 5.55381012f}, -1}}},
        {"Blast Wall", {{0, {10.7975674f, 3.68020535f}, -1}}},
        {"Moving Target", {{0, {9.8272028f, 6.24988461f}, -1}}},
        {"Wrecking Ball", {{0, {10.7289772f, 4.13915443f}, -1}}},
        {"Windmill", {{180, {10.1593046f, 4.63434315f}, -1}, {713, {9.17228508f, 4.79278755f}, -1}}},
    };
    return s;
}

}  // namespace kid::testdata
