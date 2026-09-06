#pragma once

#include "game/core/context.hpp"
#include "game/core/state.hpp"
#include "game/ui/ui_renderer.hpp"

#include <array>
#include <cstddef>

namespace game {

class MenuState final : public State {
public:
    explicit MenuState(Context context);

    StateId handle_event(const SDL_Event& event) override;
    StateId update(float delta_time) override;
    void render(float interpolation) override;

private:
    [[nodiscard]] StateId activate(std::size_t option);

    Context context_;
    UiRenderer ui_;
    SDL_Point mouse_{};
    std::size_t selected_ = 0;
    std::array<float, 3> option_focus_{1.0F, 0.0F, 0.0F};
    std::array<float, ship_count> ship_focus_{1.0F, 0.0F, 0.0F};
    float elapsed_ = 0.0F;
};

}  // namespace game
