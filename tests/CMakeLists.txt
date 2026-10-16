# Uses the engine tests' dependency-free runner (test.hpp + main.cpp) from the submodule.
set(RUNNER ${CMAKE_SOURCE_DIR}/external/physics-engine/tests)
add_executable(game_tests ${RUNNER}/main.cpp test_objectives.cpp test_level.cpp test_layouts.cpp test_progress.cpp)
target_include_directories(game_tests PRIVATE ${RUNNER})
target_link_libraries(game_tests PRIVATE kid_game)
add_test(NAME game_tests COMMAND game_tests)
