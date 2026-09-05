#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>

namespace game {

struct MusicHandle {
    std::size_t index = static_cast<std::size_t>(-1);
};

struct SoundHandle {
    std::size_t index = static_cast<std::size_t>(-1);
};

class Audio {
public:
    Audio();
    ~Audio();

    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    [[nodiscard]] bool initialise();
    [[nodiscard]] MusicHandle load_music(const std::filesystem::path& path);
    [[nodiscard]] SoundHandle load_sound(const std::filesystem::path& path);
    void play_music(MusicHandle music, int loops = -1);
    void play_sound(SoundHandle sound);
    void pause_music();
    void resume_music();
    [[nodiscard]] bool available() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace game
