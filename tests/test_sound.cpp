// The synthesised sounds: no clip clips, clicks, or comes out silent, and louder hits are louder.
#include "test.hpp"

#include <synth.hpp>

#include <cmath>

using namespace kid::sound;

namespace {

float peak(const Clip& c) {
    float m = 0;
    for (float x : c) m = std::fmax(m, std::fabs(x));
    return m;
}
float rms(const Clip& c) {
    double s = 0;
    for (float x : c) s += static_cast<double>(x) * static_cast<double>(x);
    return static_cast<float>(std::sqrt(s / static_cast<double>(c.size())));
}
// Well formed: finite, under full scale, audible, and starting and ending at silence (no click).
bool sound_ok(const Clip& c, const char* what) {
    bool ok = !c.empty() && c.size() < static_cast<size_t>(kRate) * 3;
    for (float x : c) ok = ok && std::isfinite(x);
    ok = ok && peak(c) <= 1.0f && peak(c) > 0.05f;
    ok = ok && std::fabs(c.front()) < 1e-3f && std::fabs(c.back()) < 1e-3f;
    if (!ok) std::printf("    %s: %zu samples, peak %g, ends %g %g\n", what, c.size(), static_cast<double>(peak(c)), static_cast<double>(c.front()),
                         static_cast<double>(c.back()));
    return ok;
}

}  // namespace

TEST(every_sound_is_well_formed) {
    Noise n;
    for (const Tone t : {Tone::Wood, Tone::Stone, Tone::Ice, Tone::Pig, Tone::Bird, Tone::Ground}) {
        CHECK(sound_ok(knock(t, 0.0f, n), "knock soft"));
        CHECK(sound_ok(knock(t, 1.0f, n), "knock hard"));
        CHECK(sound_ok(shatter(t, 0.35f, n), "shatter"));
    }
    CHECK(sound_ok(squeal(0.3f, n), "squeal"));
    CHECK(sound_ok(squeal(0.5f, n), "big squeal"));
    CHECK(sound_ok(boom(2.5f, n), "boom"));
    CHECK(sound_ok(boom(1.6f, n), "egg boom"));
    CHECK(sound_ok(creak(n), "creak"));
    CHECK(sound_ok(twang(0.2f, n), "twang"));
    CHECK(sound_ok(twang(1.0f, n), "full twang"));
    CHECK(sound_ok(whoosh(1.0f, n), "whoosh"));
    CHECK(sound_ok(jingle(true), "win"));
    CHECK(sound_ok(jingle(false), "fail"));
    for (int s = 0; s < 3; ++s) CHECK(sound_ok(chime(s), "chime"));
}

TEST(harder_hits_sound_louder_and_materials_sound_different) {
    Noise n;
    CHECK(rms(knock(Tone::Wood, 1.0f, n)) > 2 * rms(knock(Tone::Wood, 0.1f, n)));
    CHECK(rms(twang(1.0f, n)) > rms(twang(0.1f, n)));
    // Pitch, by counting how often the start of the clip crosses zero: ice rings far higher than wood,
    // wood higher than the ground.
    const auto crossings = [](const Clip& c) {
        int k = 0;
        for (size_t i = 1; i < c.size() / 3; ++i) k += (c[i - 1] < 0) != (c[i] < 0);
        return k / static_cast<double>(c.size() / 3);
    };
    Noise a, b, g;
    const double ice = crossings(knock(Tone::Ice, 1, a)), wood = crossings(knock(Tone::Wood, 1, b)), ground = crossings(knock(Tone::Ground, 1, g));
    CHECK(ice > 3 * wood);
    CHECK(wood > ground);
}

TEST(sounds_repeat_exactly_from_the_same_seed) {
    Noise a, b;
    CHECK(shatter(Tone::Stone, 0.5f, a) == shatter(Tone::Stone, 0.5f, b));
    CHECK(knock(Tone::Ice, 0.7f, a) == knock(Tone::Ice, 0.7f, b));
}

TEST(the_ambience_is_quiet_and_never_stops) {
    Ambience amb;
    float m = 0;
    double energy = 0;
    const int n = kRate * 20;
    for (int i = 0; i < n; ++i) {
        const float x = amb.next();
        CHECK(std::isfinite(x));
        m = std::fmax(m, std::fabs(x));
        energy += static_cast<double>(x) * static_cast<double>(x);
    }
    CHECK(m < 0.3f);  // background: it must never drown the game
    CHECK(std::sqrt(energy / n) > 1e-3);  // but it is there
}
