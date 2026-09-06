#pragma once

#include "game/core/audio.hpp"
#include "game/core/graphics.hpp"
#include "game/core/loadout.hpp"

#include <array>

namespace game {

struct Resources {
    Graphics::Texture background;
    Graphics::Texture menu;
    std::array<Graphics::Texture, ship_count> ships;
    Graphics::Texture asteroid;
    Graphics::Texture bullet;
    MusicHandle background_music;
    MusicHandle death_music;
    SoundHandle fire_sound;
    SoundHandle impact_sound;
};

}  // namespace game
