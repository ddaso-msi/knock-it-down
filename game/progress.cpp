#include "progress.hpp"

#include <sstream>

namespace kid {

bool Progress::record(const std::string& level, int stars, int score) {
    if (stars <= 0) return false;
    Best& b = levels[level];
    const bool first = b.stars == 0;
    bool improved = first;
    if (stars > b.stars) {
        b.stars = stars;
        improved = true;
    }
    if (first || score > b.score) {
        b.score = score;
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

bool unlocked(const Progress& p, const std::vector<std::string>& order, int index) {
    if (index < 0 || index >= static_cast<int>(order.size())) return false;
    return index == 0 || p.best(order[static_cast<size_t>(index - 1)]).stars > 0;
}

int next_to_play(const Progress& p, const std::vector<std::string>& order) {
    for (int i = 0; i < static_cast<int>(order.size()); ++i)
        if (p.best(order[static_cast<size_t>(i)]).stars == 0) return i;
    return order.empty() ? 0 : static_cast<int>(order.size()) - 1;
}

std::string format_progress(const Progress& p) {
    std::string out;
    for (const auto& [name, b] : p.levels)
        if (b.stars > 0) out += std::to_string(b.stars) + " " + std::to_string(b.score) + " " + name + "\n";
    return out;
}

Progress parse_progress(const std::string& text) {
    Progress p;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        std::istringstream in(line);
        int stars = 0, score = 0;
        std::string name;
        if (!(in >> stars >> score) || stars < 1 || stars > 3 || score < 0) continue;
        std::getline(in >> std::ws, name);
        if (name.empty()) continue;
        p.record(name, stars, score);
    }
    return p;
}

}  // namespace kid
