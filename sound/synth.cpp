#include "synth.hpp"

#include <algorithm>
#include <cmath>

namespace kid::sound {

namespace {

constexpr float kTau = 6.28318531f;

int samples(float seconds) { return std::max(1, static_cast<int>(seconds * static_cast<float>(kRate))); }

// A fade of a few milliseconds at both ends, so every clip starts and ends at exactly zero.
void soften(Clip& c, float ms = 3.0f) {
    const int fade = std::min(static_cast<int>(c.size() / 2), samples(ms / 1000.0f));
    for (int i = 0; i < fade; ++i) {
        const float g = static_cast<float>(i) / static_cast<float>(fade);
        c[static_cast<size_t>(i)] *= g;
        c[c.size() - 1 - static_cast<size_t>(i)] *= g;
    }
}

// Scales the clip so its loudest sample is `peak`.
void normalise(Clip& c, float peak) {
    float m = 0;
    for (float x : c) m = std::max(m, std::fabs(x));
    if (m > 0)
        for (float& x : c) x *= peak / m;
}

// A one-pole low-pass: `cutoff` in Hz.
struct LowPass {
    float a, y = 0;
    explicit LowPass(float cutoff) : a(1 - std::exp(-kTau * cutoff / static_cast<float>(kRate))) {}
    float operator()(float x) { return y += a * (x - y); }
};

// A struck object: a few decaying partials at the given ratios, plus a noise burst for the attack.
Clip strike(float base, std::initializer_list<float> ratios, float decay, float noise_level, float noise_cutoff, float seconds,
            float gain, Noise& n) {
    Clip c(static_cast<size_t>(samples(seconds)));
    LowPass lp(noise_cutoff);
    int k = 0;
    for (float r : ratios) {
        const float f = base * r, d = decay / (1 + 0.6f * static_cast<float>(k++));
        const float phase = n.uniform() * kTau;
        for (size_t i = 0; i < c.size(); ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(kRate);
            c[i] += std::sin(kTau * f * t + phase) * std::exp(-t / d) / static_cast<float>(ratios.size());
        }
    }
    for (size_t i = 0; i < c.size(); ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kRate);
        c[i] += noise_level * lp(n.white()) * std::exp(-t / 0.012f);
    }
    normalise(c, gain);
    soften(c);
    return c;
}

}  // namespace

float Noise::uniform() {
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(state >> 8) / static_cast<float>(1 << 24);
}
float Noise::white() { return uniform() * 2 - 1; }

Clip knock(Tone tone, float strength, Noise& n) {
    strength = std::clamp(strength, 0.0f, 1.0f);
    const float gain = 0.15f + 0.75f * strength, wobble = 0.94f + 0.12f * n.uniform();
    switch (tone) {
        case Tone::Wood: return strike(190 * wobble, {1.0f, 2.3f, 3.9f}, 0.045f, 0.8f, 2500, 0.16f, gain, n);
        case Tone::Stone: return strike(520 * wobble, {1.0f, 1.7f, 2.9f, 4.6f}, 0.025f, 1.2f, 6000, 0.12f, gain, n);
        case Tone::Ice: return strike(1900 * wobble, {1.0f, 2.76f, 5.4f}, 0.09f, 0.4f, 9000, 0.3f, gain * 0.8f, n);
        case Tone::Pig: return strike(120 * wobble, {1.0f, 1.5f}, 0.05f, 0.5f, 900, 0.14f, gain, n);
        case Tone::Bird: return strike(300 * wobble, {1.0f, 2.0f}, 0.03f, 0.6f, 1800, 0.1f, gain * 0.7f, n);
        case Tone::Ground: return strike(70 * wobble, {1.0f, 1.6f}, 0.06f, 1.0f, 600, 0.2f, gain, n);
    }
    return strike(190, {1.0f}, 0.05f, 0.5f, 2000, 0.15f, gain, n);
}

Clip shatter(Tone tone, float size, Noise& n) {
    // Many small knocks in a burst, falling away: the pieces of the block hitting each other.
    const float seconds = 0.18f + std::clamp(size, 0.1f, 1.0f) * 0.35f;
    Clip c(static_cast<size_t>(samples(seconds + 0.2f)));
    const int pieces = 6 + static_cast<int>(size * 20);
    for (int p = 0; p < pieces; ++p) {
        const float when = seconds * std::pow(n.uniform(), 1.6f);
        const Clip k = knock(tone, 0.3f + 0.7f * n.uniform() * (1 - when / seconds), n);
        const size_t at = static_cast<size_t>(when * static_cast<float>(kRate));
        for (size_t i = 0; i < k.size() && at + i < c.size(); ++i) c[at + i] += k[i];
    }
    normalise(c, 0.8f);
    soften(c);
    return c;
}

Clip squeal(float size, Noise& n) {
    // A squeak gliding down an octave and a half, a little vibrato, then a breathy puff.
    const float base = 1100.0f / (0.6f + size), len = 0.22f;
    Clip c(static_cast<size_t>(samples(len + 0.12f)));
    float phase = 0;
    for (size_t i = 0; i < c.size(); ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kRate);
        if (t < len) {
            const float f = base * std::pow(0.35f, t / len) * (1 + 0.03f * std::sin(kTau * 28 * t));
            phase += kTau * f / static_cast<float>(kRate);
            const float env = std::sin(3.14159f * t / len);
            c[i] += 0.6f * env * (std::sin(phase) + 0.3f * std::sin(2 * phase));
        }
        const float puff = t - len * 0.8f;
        if (puff > 0) c[i] += 0.5f * n.white() * std::exp(-puff / 0.04f);
    }
    normalise(c, 0.75f);
    soften(c);
    return c;
}

Clip boom(float radius, Noise& n) {
    const float len = 0.5f + radius * 0.25f;
    Clip c(static_cast<size_t>(samples(len)));
    LowPass rumble(180 + 60 * radius), crack(3000);
    float phase = 0;
    for (size_t i = 0; i < c.size(); ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kRate);
        const float f = 90.0f * std::exp(-t / 0.15f) + 35.0f;
        phase += kTau * f / static_cast<float>(kRate);
        const float body = std::sin(phase) * std::exp(-t / (0.12f + radius * 0.05f));
        const float tail = rumble(n.white()) * std::exp(-t / (0.18f + radius * 0.06f)) * 3.0f;
        const float snap = crack(n.white()) * std::exp(-t / 0.01f);
        c[i] = body + tail + 0.6f * snap;
    }
    normalise(c, 0.95f);
    soften(c);
    return c;
}

Clip creak(Noise& n) {
    // Rubber stretching: a burst of narrow, buzzing pulses.
    Clip c(static_cast<size_t>(samples(0.06f)));
    const float f = 80 + 40 * n.uniform();
    LowPass lp(1400);
    for (size_t i = 0; i < c.size(); ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kRate);
        const float pulse = std::fmod(t * f, 1.0f) < 0.15f ? 1.0f : 0.0f;
        c[i] = lp(pulse + 0.3f * n.white()) * std::sin(3.14159f * t / 0.06f);
    }
    normalise(c, 0.25f);
    soften(c);
    return c;
}

Clip twang(float pull, Noise& n) {
    // The band snapping back: a low pitch that wobbles and dies, and the air of the bird leaving.
    pull = std::clamp(pull, 0.0f, 1.0f);
    const float len = 0.35f;
    Clip c(static_cast<size_t>(samples(len)));
    LowPass air(1200 + 2500 * pull);
    float phase = 0;
    for (size_t i = 0; i < c.size(); ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kRate);
        const float f = (95 + 60 * pull) * (1 + 0.08f * std::sin(kTau * 9 * t) * std::exp(-t / 0.1f));
        phase += kTau * f / static_cast<float>(kRate);
        c[i] = 0.8f * std::sin(phase) * std::exp(-t / 0.09f) + air(n.white()) * std::exp(-t / 0.06f) * (0.5f + pull);
    }
    normalise(c, 0.3f + 0.5f * pull);
    soften(c);
    return c;
}

Clip whoosh(float pitch, Noise& n) {
    const float len = 0.3f;
    Clip c(static_cast<size_t>(samples(len)));
    LowPass a(400), b(400);
    for (size_t i = 0; i < c.size(); ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kRate), u = t / len;
        // Sweep a band of noise upwards: the difference of two low-passes is a band-pass.
        const float centre = (500 + 2500 * u) * pitch;
        a.a = 1 - std::exp(-kTau * centre * 1.5f / static_cast<float>(kRate));
        b.a = 1 - std::exp(-kTau * centre * 0.6f / static_cast<float>(kRate));
        const float w = n.white();
        c[i] = (a(w) - b(w)) * std::sin(3.14159f * u);
    }
    normalise(c, 0.5f);
    soften(c);
    return c;
}

Clip chime(int step) {
    // A bell: a pure tone with a faint fifth above, on a major scale.
    static constexpr int kScale[] = {0, 2, 4, 5, 7, 9, 11, 12, 14, 16};
    const float f = 660.0f * std::pow(2.0f, static_cast<float>(kScale[std::clamp(step, 0, 9)]) / 12.0f);
    Clip c(static_cast<size_t>(samples(0.6f)));
    for (size_t i = 0; i < c.size(); ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kRate);
        c[i] = (std::sin(kTau * f * t) + 0.25f * std::sin(kTau * f * 1.5f * t) + 0.1f * std::sin(kTau * f * 2.0f * t)) * std::exp(-t / 0.18f);
    }
    normalise(c, 0.45f);
    soften(c);
    return c;
}

Clip jingle(bool up) {
    // Up: four notes climbing. Down: three falling, slower, a little flat.
    const std::vector<int> notes = up ? std::vector<int>{0, 2, 4, 7} : std::vector<int>{4, 2, 0};
    const float gap = up ? 0.11f : 0.2f;
    Clip c(static_cast<size_t>(samples(gap * static_cast<float>(notes.size()) + 0.7f)));
    for (size_t k = 0; k < notes.size(); ++k) {
        Clip note = chime(notes[k]);
        if (!up) {  // detune the fall a touch by stretching it
            Clip flat(static_cast<size_t>(static_cast<float>(note.size()) * 1.06f));
            for (size_t i = 0; i < flat.size(); ++i) flat[i] = note[std::min(note.size() - 1, static_cast<size_t>(static_cast<float>(i) / 1.06f))];
            note = flat;
        }
        const size_t at = static_cast<size_t>(gap * static_cast<float>(k) * static_cast<float>(kRate));
        for (size_t i = 0; i < note.size() && at + i < c.size(); ++i) c[at + i] += note[i];
    }
    normalise(c, 0.5f);
    soften(c);
    return c;
}

float Ambience::next() {
    t_ += 1.0 / kRate;
    // Wind: noise through two low-passes, its loudness drifting slowly.
    const float gust = 0.55f + 0.45f * static_cast<float>(std::sin(t_ * 0.21) * std::sin(t_ * 0.077 + 1.3));
    wind_ += 0.002f * (noise_.white() - wind_);
    wind2_ += 0.01f * (wind_ - wind2_);
    float out = wind2_ * 1.6f * gust;
    // Now and then a bird far off: two or three quick rising whistles.
    if (chirp_left_ > 0) {
        const float u = 1 - static_cast<float>(chirp_left_) / static_cast<float>(chirp_len_);
        chirp_phase_ += kTau * chirp_freq_ * (1 + 0.5f * u) / static_cast<float>(kRate);
        out += 0.05f * std::sin(chirp_phase_) * std::sin(3.14159f * u);
        if (--chirp_left_ == 0) chirp_gap_ = noise_.uniform() < 0.6f ? samples(0.08f) : samples(4.0f + 6.0f * noise_.uniform());
    } else if (--chirp_gap_ <= 0) {
        chirp_len_ = chirp_left_ = samples(0.07f + 0.05f * noise_.uniform());
        chirp_freq_ = 2600 + 1200 * noise_.uniform();
        chirp_phase_ = 0;
    }
    return out;
}

}  // namespace kid::sound
