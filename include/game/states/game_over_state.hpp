#pragma once

#include "game/core/context.hpp"
#include "game/core/state.hpp"
#include "game/ui/ui_renderer.hpp"

#include <cstddef>

namespace game {

class GameOverState final : public State {
public:
    explicit GameOverState(Context context);

    StateId handle_event(const SDL_Event& event) override;
    StateId update(float delta_time) override;
    void render(float interpolation) override;

private:
    [[nodiscard]] StateId activate(std::size_t option) const;

    Context context_;
    UiRenderer ui_;
    float elapsed_ = 0.0F;
    SDL_Point mouse_{};
    std::size_t selected_ = 0;
};

}  // namespace game
