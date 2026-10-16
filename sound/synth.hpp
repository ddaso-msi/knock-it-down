#pragma once
// Every sound in the game, made from oscillators, noise and envelopes: there are no recordings. Each
// function returns a short mono clip at kRate samples a second, peaking below 1, and starting and
// ending at silence so it never clicks. No SDL here: the app plays the clips, the tests check them.
#include <cstdint>
#include <vector>

namespace kid::sound {

constexpr int kRate = 44100;
using Clip = std::vector<float>;

// A small random source of its own, so sounds never touch anything else's random numbers.
struct Noise {
    std::uint32_t state = 0x9e3779b9u;
    float uniform();  // [0, 1)
    float white();    // [-1, 1)
};

// What was hit, as far as the ear is concerned.
enum class Tone { Wood, Stone, Ice, Pig, Bird, Ground };

// One object striking another; `strength` 0..1 (from the impulse). Wood knocks, stone clacks, ice
// tinkles, a pig thumps, the ground thuds.
Clip knock(Tone tone, float strength, Noise& n);
// A block breaking apart: a crackle in the material's voice, longer for bigger blocks (`size` in metres).
Clip shatter(Tone tone, float size, Noise& n);
// A pig popping: a squeak sliding down, then a puff.
Clip squeal(float size, Noise& n);
// An explosion: a low thump and a noise tail, bigger for a bigger `radius` (metres).
Clip boom(float radius, Noise& n);
// The sling: a short creak while pulling, and the twang of letting go (`pull` 0..1).
Clip creak(Noise& n);
Clip twang(float pull, Noise& n);
// A power going off: a rising swish; `pitch` shifts it per bird.
Clip whoosh(float pitch, Noise& n);
// A short tune: `up` for a win (rising), otherwise a fall. A chime is one note, for a star.
Clip jingle(bool up);
Clip chime(int step);

// The background: wind that rises and falls, and now and then a distant bird. Endless; call next()
// once per sample.
class Ambience {
public:
    float next();

private:
    Noise noise_;
    double t_ = 0;
    float wind_ = 0, wind2_ = 0;
    // The current chirp, if any.
    int chirp_left_ = 0, chirp_len_ = 0, chirp_gap_ = kRate * 3;
    float chirp_freq_ = 0, chirp_phase_ = 0;
};

}  // namespace kid::sound
