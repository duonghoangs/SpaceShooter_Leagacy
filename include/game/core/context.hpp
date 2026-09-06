#pragma once

#include "game/core/audio.hpp"
#include "game/core/graphics.hpp"
#include "game/core/loadout.hpp"
#include "game/core/resources.hpp"

namespace game {

struct Context {
    Graphics& graphics;
    Audio& audio;
    Resources& resources;
    bool& sound_enabled;
    int& last_score;
    int& high_score;
    ShipType& selected_ship;
};

}  // namespace game
