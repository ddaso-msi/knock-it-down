# Knock It Down

A physics puzzle game: fire shots from a launcher into a live simulation until every orange block is
off its platform. Twelve levels, each built around one idea, on the from-scratch 2D engine in
[physics-engine](https://github.com/ddaso-msi/physics-engine). It started as the first level of
Physics Puzzle Lab.

Every level has a par, the fewest shots known to solve it. Par or better is three stars, one over is
two, any other win is one. Best results are saved per level.

| # | Level | Idea | Shots | Par |
|---|-------|------|-------|-----|
| 1 | First Shot | a pyramid on a platform | 3 balls | 2 |
| 2 | Two Shelves | two platforms: one shot cannot reach both | 3 balls | 2 |
| 3 | Over the Wall | lob over a wall onto a tall column | 3 balls | 1 |
| 4 | Weak Point | knock a leg out from under a plank bridge | 3 balls | 1 |
| 5 | Ice Shelf | ice hardly grips: a nudge slides a block a long way | 3 balls | 2 |
| 6 | Heavy Going | stone slabs: a ball bounces off, the heavy shot shoves | heavy + 2 balls | 1 |
| 7 | Hands Off | purple blocks must stay up; lose one and the level fails | 3 balls | 2 |
| 8 | Domino Run | the blocks sit in a garage only dominoes can reach | 2 balls | 1 |
| 9 | Blast Wall | behind a wall and under a roof: only a blast gets there | 2 blasts | 1 |
| 10 | Moving Target | the platform glides back and forth | 3 balls | 1 |
| 11 | Wrecking Ball | swing a hanging stone ball into a shielded stack | 3 balls | 1 |
| 12 | Windmill | a motor-driven bar guards the way in: time the shot | 3 balls | 2 |

## Build and play

Needs CMake 3.25+, Ninja, a C++20 compiler, and SDL3 for the game window (`brew install sdl3`). The
engine is a git submodule:

```bash
git clone --recursive <this repo>          # or, in an existing clone: git submodule update --init
cmake --preset release && cmake --build --preset release
build/release/knock_it_down
```

Aim with the mouse (the dots show the path; aiming further from the launcher fires faster) and click
to fire. A click while a blast is in flight sets it off. `R` resets, `Tab` pauses, `H` shows the help,
`C` shows contact points, `N` goes to the next level once solved, `Esc` goes back to the menu.

## Tests

```bash
cmake --preset debug && cmake --build --preset debug && ctest --preset debug     # ASan + UBSan, about 20 s
```

The tests cover the rules on small hand-built layouts: shots and reloading, per-platform fall lines,
keeps, the win hold, running out of shots, blasts, motors, sliders, stars, and saved progress. They
also check every level:
- it is well formed, and nothing in it overlaps at the start;
- it stands up by itself for 10 seconds;
- its recorded par solution (`tests/solutions.hpp`) still wins with three stars when replayed step for step;
- what it claims to the player holds. No shot reaches Domino Run's blocks or Wrecking Ball's stack
  without first touching the dominoes or the wrecking ball. No ordinary shot moves Blast Wall's blocks.
  No single ball clears Heavy Going.

Each test was checked by breaking the code it guards in a scratch copy (for example, a blast without
falloff, a fall line above the platform, or a roof too short) and confirming the test fails.

The build turns off fused multiply-add (`-ffp-contract=off`), so a recorded solution replays bit for
bit in both the Debug and the Release build.

## How the levels were checked: `kid_solve`

```bash
build/release/kid_solve --level 11                  # search level 11
build/release/kid_solve --level 12 --waits 0,0.3,0.6,0.9,1.2,1.5   # with timing, for moving parts
build/release/kid_solve --level 6 --ammo bbb        # could it be done with balls only? (b, h heavy, x blast)
```

The solver tries every shot on a grid (1 degree by 0.5 m/s) from the level's start. It keeps the best
few results and tries the grid again from each, one shot deeper each time. For each number of shots
it prints how many tries win, and how many are robust: their four grid neighbours win too. It prints
the most robust winning sequence as exact fire steps, ready to paste into `tests/solutions.hpp`, and
how often that sequence still wins with every aim nudged up to 2 cm at random. Par was set from these
numbers: the fewest shots at which wins are not a fluke.

A Level is plain data, so the solver copies a game mid-run and branches it at every shot.

## Layout

```
external/physics-engine   the engine (submodule, pinned to its main branch)
game/                     rules, no SDL: layout.hpp (levels as data), level.hpp (playing one), progress.hpp
game/levels/levels.cpp    the twelve levels
app/                      the SDL3 game: window, input, drawing, saved progress
tools/solve.cpp           kid_solve
tests/                    game_tests, using the engine's dependency-free test runner
```

Adding a level: write a function returning a `Layout` in `game/levels/levels.cpp` and add it to
`kLevels`. Run `kid_solve` on it to find its par and a solution, paste that into `tests/solutions.hpp`,
then fix the level-count test.

The engine's continuous collision only covers static bodies, so the top shot speed (20 m/s, 17 cm a
step) is kept well under the thinnest piece plus a ball.
