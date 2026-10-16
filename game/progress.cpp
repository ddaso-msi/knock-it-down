#include "progress.hpp"

#include <sstream>

namespace kid {

bool Progress::record(const std::string& level, int stars, int shots) {
    if (stars <= 0) return false;
    Best& b = levels[level];
    const bool first = b.stars == 0;
    bool improved = first;
    if (stars > b.stars) {
        b.stars = stars;
        improved = true;
    }
    if (first || shots < b.shots) {
        b.shots = shots;
        improved = true;
    }
    return improved;
}

Best Progress::best(const std::string& level) const {
    const auto it = levels.find(level);
    return it == levels.end() ? Best{} : it->second;
}

int Progress::total_stars() const {
    int n = 0;
    for (const auto& [name, b] : levels) n += b.stars;
    return n;
}

std::string format_progress(const Progress& p) {
    std::string out;
    for (const auto& [name, b] : p.levels)
        if (b.stars > 0) out += std::to_string(b.stars) + " " + std::to_string(b.shots) + " " + name + "\n";
    return out;
}

Progress parse_progress(const std::string& text) {
    Progress p;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        std::istringstream in(line);
        int stars = 0, shots = 0;
        std::string name;
        if (!(in >> stars >> shots) || stars < 1 || stars > 3 || shots < 1) continue;
        std::getline(in >> std::ws, name);
        if (name.empty()) continue;
        p.record(name, stars, shots);
    }
    return p;
}

}  // namespace kid
