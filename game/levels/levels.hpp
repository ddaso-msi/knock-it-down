#pragma once
// The levels, in menu order. Each one is a Layout built in levels.cpp.
#include <layout.hpp>

namespace kid {

int level_count();
// Level `index` (0-based). Out of range gives an empty layout with no name.
Layout make_layout(int index);

}  // namespace kid
