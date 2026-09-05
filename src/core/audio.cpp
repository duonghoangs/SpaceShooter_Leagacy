#include "game/core/audio.hpp"

#ifndef GAME_HAS_AUDIO
#define GAME_HAS_AUDIO 0
#endif

#if GAME_HAS_AUDIO
#include <SDL2/SDL.h>
#if __has_include(<SDL2/SDL_mixer.h>)
#include <SDL2/SDL_mixer.h>
#else
#include <SDL2_mixer/SDL_mixer.h>
#endif
#endif

#include <string>
#include <vector>

namespace game {

struct Audio::Impl {
    bool ready = false;
#if GAME_HAS_AUDIO
    std::vector<Mix_Music*> music;
    std::vector<Mix_Chunk*> sounds;
#endif
};

Audio::Audio() : impl_(std::make_unique<Impl>()) {}

Audio::~Audio() {
#if GAME_HAS_AUDIO
    for (Mix_Chunk* sound : impl_->sounds) {
        Mix_FreeChunk(sound);
    }
    for (Mix_Music* music : impl_->music) {
        Mix_FreeMusic(music);
    }
    if (impl_->ready) {
        Mix_CloseAudio();
        Mix_Quit();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
#endif
}

bool Audio::initialise() {
#if GAME_HAS_AUDIO
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        SDL_Log("Audio disabled: %s", SDL_GetError());
        return false;
    }
    Mix_Init(MIX_INIT_MP3);
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
        SDL_Log("Audio disabled: %s", Mix_GetError());
        Mix_Quit();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }
    impl_->ready = true;
    return true;
#else
    return false;
#endif
}

MusicHandle Audio::load_music(const std::filesystem::path& path) {
#if GAME_HAS_AUDIO
    if (!impl_->ready || !std::filesystem::is_regular_file(path)) {
        return {};
    }
    const std::string filename = path.string();
    Mix_Music* music = Mix_LoadMUS(filename.c_str());
    if (music == nullptr) {
        SDL_Log("Cannot load music %s: %s", filename.c_str(), Mix_GetError());
        return {};
    }
    impl_->music.push_back(music);
    return {impl_->music.size() - 1};
#else
    (void)path;
    return {};
#endif
}

SoundHandle Audio::load_sound(const std::filesystem::path& path) {
#if GAME_HAS_AUDIO
    if (!impl_->ready || !std::filesystem::is_regular_file(path)) {
        return {};
    }
    const std::string filename = path.string();
    Mix_Chunk* sound = Mix_LoadWAV(filename.c_str());
    if (sound == nullptr) {
        SDL_Log("Cannot load sound %s: %s", filename.c_str(), Mix_GetError());
        return {};
    }
    impl_->sounds.push_back(sound);
    return {impl_->sounds.size() - 1};
#else
    (void)path;
    return {};
#endif
}

void Audio::play_music(MusicHandle music, int loops) {
#if GAME_HAS_AUDIO
    if (impl_->ready && music.index < impl_->music.size()) {
        Mix_PlayMusic(impl_->music[music.index], loops);
    }
#else
    (void)music;
    (void)loops;
#endif
}

void Audio::play_sound(SoundHandle sound) {
#if GAME_HAS_AUDIO
    if (impl_->ready && sound.index < impl_->sounds.size()) {
        Mix_PlayChannel(-1, impl_->sounds[sound.index], 0);
    }
#else
    (void)sound;
#endif
}

void Audio::pause_music() {
#if GAME_HAS_AUDIO
    if (impl_->ready) {
        Mix_PauseMusic();
    }
#endif
}

void Audio::resume_music() {
#if GAME_HAS_AUDIO
    if (impl_->ready) {
        Mix_ResumeMusic();
    }
#endif
}

bool Audio::available() const {
    return impl_->ready;
}

}  // namespace game
