#pragma once
// Sound: a small mixer that keeps SDL's audio stream fed, and a soundboard that turns what happened in
// the level (hits, pops, blasts, powers, the sling) into the synthesised clips of sound/synth.hpp.
#include <SDL3/SDL.h>
#include <level.hpp>
#include <synth.hpp>

#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace kid::app {

class Audio {
public:
    ~Audio() { close(); }
    // Opens the default output. Without one (no device, a headless run) everything stays silent.
    bool open();
    void close();
    // Mixes a clip in now. `pan` is -1 (left) to 1 (right).
    void play(const sound::Clip& clip, float gain = 1, float pan = 0);
    // Tops up the device's queue to a few tens of milliseconds ahead. Call once a frame.
    void pump();
    bool muted = false;
    // Instead of a device: mix into memory, `seconds` at a time, and save it all as a WAV file at the
    // end. The scripted run uses this to check what a shot sounds like.
    void record();
    void advance(float seconds);
    bool save_wav(const char* path) const;

private:
    SDL_AudioStream* stream_ = nullptr;
    bool recording_ = false;
    double owed_ = 0;                  // recording: samples due but not yet mixed
    std::vector<float> recorded_;      // interleaved stereo
    void mix(float* out, int frames);
    std::vector<float> left_, right_;  // what is still to be played, mixed
    sound::Ambience ambience_;
};

class Soundboard {
public:
    // Plays whatever the level has done since the last call. `pull` is the sling's pull right now.
    void update(const Level& level, Audio& audio, std::optional<Vec2> pull);
    void clear();
    // Things the level does not log: the stars of a win, one at a time.
    void star(Audio& audio, int which);

private:
    size_t hits_ = 0, pops_ = 0, booms_ = 0, powers_ = 0, shots_ = 0;
    Status status_ = Status::Playing;
    float creaked_at_ = 0;  // pull length at the last creak
    std::map<std::pair<int, int>, double> last_hit_;  // a pair that keeps touching is heard once
    double time_ = 0;
    sound::Noise noise_;
};

}  // namespace kid::app
