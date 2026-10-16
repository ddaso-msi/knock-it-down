#pragma once
// The player's best result on each level, and its save format. Pure data and text: the app decides
// where the file lives and reads and writes it.
#include <map>
#include <string>

namespace kid {

struct Best {
    int stars = 0;  // 1 to 3 once solved
    int score = 0;  // highest score in a win
};

struct Progress {
    // Keyed by level name, so reordering the levels does not shuffle anyone's stars.
    std::map<std::string, Best> levels;

    // Keeps the better of the old and the new result. Returns whether anything improved.
    bool record(const std::string& level, int stars, int score);
    Best best(const std::string& level) const;
    int total_stars() const;
};

// One line per solved level: "stars score name". Lines that do not parse are skipped, so a damaged
// file costs at most the lines that are damaged.
std::string format_progress(const Progress& p);
Progress parse_progress(const std::string& text);

}  // namespace kid
