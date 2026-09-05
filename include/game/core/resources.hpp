#pragma once

#include "game/core/audio.hpp"
#include "game/core/graphics.hpp"

namespace game {

struct Resources {
    Graphics::Texture background;
    Graphics::Texture menu;
    Graphics::Texture ship;
    Graphics::Texture asteroid;
    Graphics::Texture bullet;
    MusicHandle background_music;
    MusicHandle death_music;
    SoundHandle fire_sound;
    SoundHandle impact_sound;
};

}  // namespace game
