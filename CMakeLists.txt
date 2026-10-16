cmake_minimum_required(VERSION 3.25)
project(knock_it_down LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# No fused multiply-add: a recorded solution has to replay bit for bit in every build type, and
# contraction is the one thing that would let a Debug and a Release build round differently.
add_compile_options(-Wall -Wextra -Wconversion -Wshadow -ffp-contract=off)

option(KID_SANITIZE "Build with ASan + UBSan" OFF)
if(KID_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

if(NOT EXISTS ${CMAKE_SOURCE_DIR}/external/physics-engine/engine/include/phys/world.hpp)
  message(FATAL_ERROR "The engine submodule is missing. Run: git submodule update --init")
endif()

# The 2D engine, compiled straight from the submodule. Only its library is built here: the engine's own
# tests, benchmarks and demo stay in the engine repo.
set(ENGINE ${CMAKE_SOURCE_DIR}/external/physics-engine/engine)
add_library(phys STATIC ${ENGINE}/src/shapes.cpp ${ENGINE}/src/body.cpp ${ENGINE}/src/collision.cpp ${ENGINE}/src/solver.cpp
            ${ENGINE}/src/joints.cpp ${ENGINE}/src/dynamic_tree.cpp ${ENGINE}/src/broadphase.cpp ${ENGINE}/src/world.cpp)
target_include_directories(phys PUBLIC ${ENGINE}/include)

# Game rules: levels, shots, objectives, scoring. No SDL, so the tests and the solver play headless.
add_library(kid_game STATIC game/level.cpp game/layout.cpp game/progress.cpp game/levels/levels.cpp)
target_include_directories(kid_game PUBLIC game)
target_link_libraries(kid_game PUBLIC phys)

# The game's sounds, synthesised: no SDL, so the tests can check them.
add_library(kid_synth STATIC sound/synth.cpp)
target_include_directories(kid_synth PUBLIC sound)

# Searches a level for winning shots: how levels are checked to be solvable, and how par is found.
add_executable(kid_solve tools/solve.cpp)
target_link_libraries(kid_solve PRIVATE kid_game)

enable_testing()
add_subdirectory(tests)

find_package(SDL3 CONFIG QUIET)
if(SDL3_FOUND)
  add_executable(knock_it_down app/main.cpp app/render.cpp app/scene.cpp app/effects.cpp app/audio.cpp app/camera.cpp)
  target_link_libraries(knock_it_down PRIVATE kid_game kid_synth SDL3::SDL3)
else()
  message(STATUS "SDL3 not found, skipping the game itself")
endif()
