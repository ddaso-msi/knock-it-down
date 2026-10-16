# Sling

A slingshot physics game (working title) on the from-scratch 2D engine in
[physics-engine](https://github.com/ddaso-msi/physics-engine). Pull a bird back in the sling, let go,
and bring down structures of wood, ice and stone until every pig has popped. It grew out of Knock It
Down, which is on the `main` branch of this repo; this is the `sling` branch.

No art: everything is drawn from plain shapes. The names and the look are our own.

## How it plays

- **The sling.** Press near the sling, drag the bird back and let go. It flies the opposite way, faster
  the further you pull (up to 20 m/s). The dots show its path while you aim.
- **Damage.** Every contact that was closing faster than 1 m/s hurts both bodies by the impulse it
  took to stop it. That is the impact's real strength, so a heavy bird hits harder than a light one
  at the same speed, and a falling block hurts whatever it lands on. Resting weight never hurts,
  however tall the stack. A block or pig pops when its health runs out.
- **Materials.** Ice is brittle and slippery, wood takes a knock, and stone takes a beating. Health
  scales with a block's size.
- **Pigs.** A small pig pops from a fall of about a metre. A helmet pig takes a few hits. A big pig
  survives any single bird head-on.
- **Birds** (powers come in stage 2):
  - ball: the ordinary bird;
  - heavy: five times the mass;
  - blast: explodes on impact or when clicked, pushing and damaging everything within 2.5 m, even
    through walls.
- **Winning.** Pop every pig. The score is everything broken, plus 10,000 for each bird left over.
  Stars are against par, the fewest birds known to solve the level.

Round things have a little rolling resistance, which the engine itself does not model. Without it, a
pig on a plank tilted by a tenth of a degree rolls off in time.

| # | Level | Idea | Birds | Par |
|---|-------|------|-------|-----|
| 1 | First Pig | one pig on a wooden table | 3 balls | 1 |
| 2 | Glass House | ice: the house comes down on the pig inside | 3 balls | 1 |
| 3 | Wood Fort | a two-storey fort, a pig on every floor | 3 balls | 1 |
| 4 | Stone Wall | stone in front of a helmet pig: the heavy bird gets through | heavy + 2 balls | 1 |
| 5 | Two Towers | two towers and a big pig between them | 4 balls | 2 |
| 6 | Bunker | a big pig in concrete: only blasts reach it | 2 blasts + ball | 2 |

## Build and play

Needs CMake 3.25+, Ninja, a C++20 compiler, and SDL3 for the game window (`brew install sdl3`). The
engine is a git submodule:

```bash
git clone --recursive <this repo> && git checkout sling     # or: git submodule update --init
cmake --preset release && cmake --build --preset release
build/release/knock_it_down
```

`R` resets, `Tab` pauses, `H` shows the help, `C` shows contact points, `N` goes to the next level once
solved, `Esc` lets a pulled bird back down, then goes back to the menu. Best results are saved per
level.

## Tests

```bash
cmake --preset debug && cmake --build --preset debug && ctest --preset debug     # ASan + UBSan
```

The rules are tested on small hand-built layouts: the sling's clamps, damage only from real impacts,
ice before wood before stone, pigs and falls, popping and scoring, the bird bonus, blasts, motors,
sliders, and joints letting go when their piece pops. Every level is tested too:
- nothing overlaps at the start;
- it stands for 10 seconds without a scratch;
- its recorded par solution (`tests/solutions.hpp`) still wins with three stars, replayed step for step;
- its claims hold. In Stone Wall the heavy bird gets through far more often than a ball. In Bunker
  nothing but a blast touches the big pig.

Each test was checked by breaking the code it guards in a scratch copy and confirming the test fails.

The build turns off fused multiply-add (`-ffp-contract=off`), so a recorded solution replays bit for
bit in both builds.

## Checking a level: `kid_solve`

```bash
build/release/kid_solve --level 4                      # search level 4
build/release/kid_solve --level 6 --detonate 0,0.6,0.8 # also try setting blasts off in flight
build/release/kid_solve --level 4 --ammo bbb           # could it be done with balls only? (b, h heavy, x blast)
```

The solver tries every shot on a grid (1 degree by 0.5 m/s), keeps the best few results (most pigs
popped), and tries the grid again from each, one bird deeper each time. It prints how many tries win
at each depth, the most robust winning sequence (as exact pulls and fire steps for
`tests/solutions.hpp`), and how often that sequence still wins with every pull nudged up to 1 cm.

## Layout

```
external/physics-engine   the engine (submodule, pinned to its main branch)
game/                     rules, no SDL: layout.hpp (levels as data), level.hpp (playing one), progress.hpp
game/levels/levels.cpp    the levels
app/                      the SDL3 game: window, input, drawing, saved progress
tools/solve.cpp           kid_solve
tests/                    game_tests, using the engine's dependency-free test runner
```

The engine's continuous collision only covers static bodies, so the top speed (20 m/s, 17 cm a step)
is kept well under the thinnest piece plus a bird.
