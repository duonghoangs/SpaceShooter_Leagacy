#pragma once

#include <SDL2/SDL.h>

namespace game {

enum class StateId { none, menu, playing, game_over, quit };

class State {
public:
    virtual ~State() = default;
    virtual StateId handle_event(const SDL_Event& event) = 0;
    virtual StateId update(float delta_time) = 0;
    virtual void render(float interpolation) = 0;
};

}  // namespace game
