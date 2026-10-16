#include "audio.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace kid::app {

using sound::Clip;
using sound::kRate;
using sound::Tone;

namespace {

constexpr int kAhead = kRate / 20;  // keep 50 ms queued: short enough to feel immediate
constexpr float kAmbienceGain = 0.35f;

float pan_of(Vec2 p) { return std::clamp((p.x / kWorldW) * 2 - 1, -1.0f, 1.0f) * 0.6f; }

Tone tone_of(Level::Surface s) {
    switch (s) {
        case Level::Surface::Wood: return Tone::Wood;
        case Level::Surface::Stone: return Tone::Stone;
        case Level::Surface::Ice: return Tone::Ice;
        case Level::Surface::Pig: return Tone::Pig;
        case Level::Surface::Bird: return Tone::Bird;
        case Level::Surface::Static: return Tone::Ground;
    }
    return Tone::Ground;
}

// Which of the two surfaces the ear picks out: the brighter one.
int loudness(Level::Surface s) {
    switch (s) {
        case Level::Surface::Ice: return 5;
        case Level::Surface::Stone: return 4;
        case Level::Surface::Wood: return 3;
        case Level::Surface::Pig: return 2;
        case Level::Surface::Bird: return 1;
        case Level::Surface::Static: return 0;
    }
    return 0;
}

}  // namespace

bool Audio::open() {
    const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, kRate};
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (!stream_) {
        SDL_Log("no sound: %s", SDL_GetError());
        return false;
    }
    SDL_ResumeAudioStreamDevice(stream_);
    return true;
}

void Audio::close() {
    if (stream_) SDL_DestroyAudioStream(stream_);
    stream_ = nullptr;
}

void Audio::play(const Clip& clip, float gain, float pan) {
    if ((!stream_ && !recording_) || muted) return;
    if (left_.size() < clip.size()) {
        left_.resize(clip.size(), 0);
        right_.resize(clip.size(), 0);
    }
    const float l = gain * std::sqrt(0.5f * (1 - pan)), r = gain * std::sqrt(0.5f * (1 + pan));  // equal power
    for (size_t i = 0; i < clip.size(); ++i) {
        left_[i] += clip[i] * l;
        right_[i] += clip[i] * r;
    }
}

void Audio::pump() {
    if (!stream_) return;
    const int queued = SDL_GetAudioStreamQueued(stream_) / static_cast<int>(2 * sizeof(float));
    const int need = kAhead - queued;
    if (need <= 0) return;
    std::vector<float> out(static_cast<size_t>(need) * 2);
    mix(out.data(), need);
    SDL_PutAudioStreamData(stream_, out.data(), static_cast<int>(out.size() * sizeof(float)));
}

void Audio::record() { recording_ = true; }

void Audio::advance(float seconds) {
    if (!recording_) return;
    owed_ += static_cast<double>(seconds) * kRate;
    const int frames = static_cast<int>(owed_);
    owed_ -= frames;
    const size_t at = recorded_.size();
    recorded_.resize(at + static_cast<size_t>(frames) * 2);
    mix(recorded_.data() + at, frames);
}

bool Audio::save_wav(const char* path) const {
    // 16-bit PCM, stereo: the plainest WAV there is.
    std::FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    const auto u32 = [&](std::uint32_t v) { std::fwrite(&v, 4, 1, f); };
    const auto u16 = [&](std::uint16_t v) { std::fwrite(&v, 2, 1, f); };
    const std::uint32_t bytes = static_cast<std::uint32_t>(recorded_.size() * 2);
    std::fwrite("RIFF", 1, 4, f);
    u32(36 + bytes);
    std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16);
    u16(1);
    u16(2);
    u32(kRate);
    u32(kRate * 4);
    u16(4);
    u16(16);
    std::fwrite("data", 1, 4, f);
    u32(bytes);
    for (float x : recorded_) u16(static_cast<std::uint16_t>(static_cast<std::int16_t>(std::lround(std::clamp(x, -1.0f, 1.0f) * 32767.0f))));
    return std::fclose(f) == 0;
}

void Audio::mix(float* out, int need) {
    for (int i = 0; i < need; ++i) {
        const size_t k = static_cast<size_t>(i);
        const float bed = muted ? 0 : ambience_.next() * kAmbienceGain;
        const float l = (k < left_.size() ? left_[k] : 0) + bed, r = (k < right_.size() ? right_[k] : 0) + bed;
        out[k * 2] = std::tanh(l);  // a soft limit, so a pile of crashes saturates instead of clipping
        out[k * 2 + 1] = std::tanh(r);
    }
    const size_t used = std::min(left_.size(), static_cast<size_t>(need));
    left_.erase(left_.begin(), left_.begin() + static_cast<long>(used));
    right_.erase(right_.begin(), right_.begin() + static_cast<long>(used));
}

void Soundboard::clear() {
    hits_ = pops_ = booms_ = powers_ = shots_ = 0;
    status_ = Status::Playing;
    creaked_at_ = 0;
    last_hit_.clear();
    time_ = 0;
}

void Soundboard::star(Audio& audio, int which) { audio.play(sound::chime(which * 2), 0.8f); }

void Soundboard::update(const Level& level, Audio& audio, std::optional<Vec2> pull) {
    if (level.sim_time < time_ || level.hits.size() < hits_ || level.shots.size() < shots_) clear();  // the level was reset
    time_ = level.sim_time;

    // Impacts: the strongest few since last time, and each pair of bodies at most every 80 ms (a block
    // rattling against its neighbour would otherwise buzz).
    std::vector<const Level::Hit*> fresh;
    for (; hits_ < level.hits.size(); ++hits_) {
        const Level::Hit& h = level.hits[hits_];
        if (h.impulse < 0.3f) continue;
        const auto key = std::minmax(h.a, h.b);
        const auto it = last_hit_.find(key);
        if (it != last_hit_.end() && h.time - it->second < 0.08) continue;
        last_hit_[key] = h.time;
        fresh.push_back(&h);
    }
    std::sort(fresh.begin(), fresh.end(), [](const Level::Hit* a, const Level::Hit* b) { return a->impulse > b->impulse; });
    for (size_t i = 0; i < fresh.size() && i < 6; ++i) {
        const Level::Hit& h = *fresh[i];
        const Level::Surface s = loudness(h.sa) >= loudness(h.sb) ? h.sa : h.sb;
        const float strength = std::clamp(h.impulse / 8.0f, 0.0f, 1.0f);
        audio.play(sound::knock(tone_of(s), strength, noise_), 0.4f + 0.6f * strength, pan_of(h.pos));
    }

    for (; pops_ < level.pops.size(); ++pops_) {
        const Level::Pop& p = level.pops[pops_];
        if (p.kind == Kind::Pig) audio.play(sound::squeal(p.size, noise_), 0.9f, pan_of(p.pos));
        else {
            const Tone t = p.material == Material::Stone ? Tone::Stone : p.material == Material::Ice ? Tone::Ice : Tone::Wood;
            audio.play(sound::shatter(t, p.size, noise_), 0.7f, pan_of(p.pos));
        }
    }
    for (; booms_ < level.booms.size(); ++booms_) audio.play(sound::boom(level.booms[booms_].radius, noise_), 1.0f, pan_of(level.booms[booms_].pos));
    for (; powers_ < level.powers_used.size(); ++powers_) {
        const Level::PowerUse& p = level.powers_used[powers_];
        const float pitch = p.kind == Ammo::Dart ? 1.6f : p.kind == Ammo::Split ? 1.2f : 0.8f;
        if (p.kind != Ammo::Blast) audio.play(sound::whoosh(pitch, noise_), 0.8f, pan_of(p.pos));  // a bomber is heard as its boom
    }

    // The sling: a creak for every bit more it is pulled, a twang when it lets go.
    const float stretch = pull ? pull->length() / Level::kMaxPull : 0;
    if (stretch > creaked_at_ + 0.12f) {
        audio.play(sound::creak(noise_), 0.5f + 0.5f * stretch, -0.6f);
        creaked_at_ = stretch;
    }
    if (!pull) creaked_at_ = 0;
    for (; shots_ < level.shots.size(); ++shots_) {
        const Body& bird = level.world.bodies[static_cast<size_t>(level.shots[shots_])];
        audio.play(sound::twang(bird.vel.length() / (Level::kMaxPull * Level::kPullSpeed), noise_), 1.0f, -0.6f);
    }

    if (level.status != status_) {
        if (level.status == Status::Won) audio.play(sound::jingle(true), 0.8f);
        if (level.status == Status::Failed) audio.play(sound::jingle(false), 0.7f);
        status_ = level.status;
    }
}

}  // namespace kid::app
