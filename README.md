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
- **Birds.** A power is used by clicking anywhere while the bird is still in flight, before it first
  touches anything. Until then the sling waits.

  | Bird | Power | Hardest on |
  |------|-------|------------|
  | basic | none | (no favourite) |
  | heavy | none: five times the mass of a basic bird | stone (x2) |
  | bomber | explodes on impact or on click: pushes and damages everything within 2.5 m, even through walls | (blast) |
  | triple | splits into three, fanned 11 degrees apart, at the same speed | ice (x3); soft on wood and stone |
  | dart | dashes forward at 30 m/s | wood (x3); soft on stone |
  | dropper | drops an egg straight down (a small blast, 1.6 m) and hops up and away | (blast) |

  Pigs take every bird's damage as it comes. The dart's 30 m/s needs no smaller time step. A bird
  only gets through a block in one step if its centre crosses the block's middle, which takes its
  radius plus the block's half width (34 cm for the thinnest post) in a step; a dash covers 25 cm. A
  test fires darts at full dash into a near-immovable 24 cm post to check this.
- **Winning.** Pop every pig. The score is everything broken, plus 10,000 for each bird left over.
  Stars come from the score. Three stars is the score of a typical win at par (the fewest birds
  known to solve the level), taken from the solver's winning tries. Two stars is one bird's bonus
  less, and any win is one star.

What you see and hear on top of the physics is only drawn and played: the simulation and the
recorded solutions are untouched by it.
- **The camera** shows the whole arena while you aim. Once a bird is away it frames the bird
  together with everything still standing, moving in as close as that allows. It holds on the crash,
  then eases back out.
- **Hard hits** shake the view; the hardest, every blast and every popped pig freeze the picture for
  an instant.
- **Birds** stretch with speed and squash when they hit, and the sling's bands quiver after a shot.
- **Sound** is synthesised from what happens, with no recordings: knocks in the voice of each
  material, scaled by the impact, breaking blocks, squealing pigs, booms, the sling's creak and
  twang, a swish for each power, a jingle and a chime per star, and wind with distant birds
  underneath. `M` mutes it.
- Popped pieces burst into debris in their own colours, and their points float up.
- The last bird's path stays on screen as dots (the one before it fainter), to aim the next by.
- A bird above the top of the screen shows as a marker under the bar, with its height.
- The winning pop plays in slow motion. Each bird left over counts up from the sling, then the
  win panel appears.

Round things have a little rolling resistance, which the engine itself does not model. Without it, a
pig on a plank tilted by a tenth of a degree rolls off in time.

| # | Level | Idea | Birds | Par | 2 / 3 stars |
|---|-------|------|-------|-----|-------------|
| 1 | First Pig | one pig on a wooden table | 3 basic | 1 | 15000 / 25000 |
| 2 | Glass House | ice: the house comes down on the pig inside | 3 basic | 1 | 21000 / 31000 |
| 3 | Wood Fort | a two-storey fort, a pig on every floor | 3 basic | 1 | 27000 / 37000 |
| 4 | Stone Wall | stone in front of a helmet pig: the heavy bird gets through | heavy + 2 basic | 1 | 24000 / 34000 |
| 5 | Two Towers | two towers and a big pig between them | 4 basic | 2 | 36000 / 46000 |
| 6 | Bunker | a big pig in concrete: only blasts reach it | 2 bombers + basic | 2 | 15000 / 25000 |
| 7 | Ice Palace | three ice rooms: split the triple | 2 triples + basic | 1 | 37000 / 47000 |
| 8 | Timber Yard | a wall of big wooden blocks: dash through it | 2 darts + basic | 2 | 16000 / 26000 |
| 9 | Rooftops | pigs in stone huts: drop eggs on the roofs | 3 droppers | 2 | 15000 / 25000 |
| 10 | Mixed Flock | stone, wood and ice in one fort, one bird for each | heavy, dart, triple, bomber | 2 | 40000 / 50000 |
| 11 | Swing Set | a roofed fort only a wrecking ball can reach | heavy + 2 basic | 2 | 16000 / 26000 |
| 12 | Windmill | a turning bar guards the way in: time the shot | dart, triple, basic | 2 | 12000 / 22000 |
| 13 | Moving Day | the pigs' hut rides a gliding platform | 3 basic | 1 | 21000 / 31000 |
| 14 | High Castle | a castle on a ledge, a material to each floor | heavy, dropper, triple, bomber | 2 | 40000 / 50000 |
| 15 | Grand Finale | three towers, six pigs, five powers | dart, triple, bomber, heavy, dropper | 2 | 62000 / 72000 |

## Build and play

Needs CMake 3.25+, Ninja, a C++20 compiler, and SDL3 for the game window (`brew install sdl3`). The
engine is a git submodule:

```bash
git clone --recursive <this repo> && git checkout sling     # or: git submodule update --init
cmake --preset release && cmake --build --preset release
build/release/knock_it_down
```

`R` resets, `Tab` pauses, `H` shows the help, `C` shows contact points, `M` mutes, `N` goes to the next level once
solved, `Esc` lets a pulled bird back down, then goes back to the menu. Best results are saved per
level.

## Tests

```bash
cmake --preset debug && cmake --build --preset debug && ctest --preset debug     # ASan + UBSan
```

The rules are tested on small hand-built layouts: the sling's clamps, damage only from real impacts,
ice before wood before stone, pigs and falls, popping and scoring, the bird bonus, blasts, motors,
sliders, joints letting go when their piece pops, and each power: only in flight and only once,
the triple's fan, the dart's dash (and that it cannot tunnel), the dropper's egg through a roof, and
each bird being hardest on its own material. Every level is tested too:
- nothing overlaps at the start;
- it stands for 10 seconds without a scratch;
- its recorded par solution (`tests/solutions.hpp`) still wins with three stars, replayed step for step;
- its claims hold:
  - Stone Wall: the heavy bird gets through far more often than a ball.
  - Bunker: nothing but a blast touches the big pig.
  - Ice Palace: splitting the triple clears three rooms nearly four times as often as not splitting.
  - Timber Yard: a dashing dart breaks the wall three times as often as a basic bird.
  - Rooftops: an egg dropped over a hut pops its pig far more often than a bird thrown at it.
  - Swing Set: on a one-degree grid, no shot touches the fort before the wrecking ball.
  - Windmill and Moving Day: the bar turns at its speed and the deck follows its swing inside the
    real level.

Each test was checked by breaking the code it guards in a scratch copy and confirming the test fails.

The sounds are tested too: no clip clips, clicks or comes out silent, harder hits are louder, ice
rings higher than wood and wood higher than the ground, and the background stays quiet. The game's
script mode records what it would play (`--sound FILE.wav`), to check the mix without a speaker.

The build turns off fused multiply-add (`-ffp-contract=off`), so a recorded solution replays bit for
bit in both builds.

## Checking a level: `kid_solve`

```bash
build/release/kid_solve --level 4                      # search level 4
build/release/kid_solve --level 7 --powers 0,0.4,0.8   # when to use the power (seconds after firing; 0 never)
build/release/kid_solve --level 4 --ammo bbb           # other birds? (b basic, h heavy, x bomber, s triple, d dart, e dropper)
```

The solver tries every shot on a grid (1 degree by 0.5 m/s), keeps the best few results (most pigs
popped), and tries the grid again from each, one bird deeper each time. It prints how many tries win
at each depth, the most robust winning sequence (as exact pulls, fire steps and power steps for
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
