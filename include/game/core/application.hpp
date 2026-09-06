#pragma once

#include "game/core/assets.hpp"
#include "game/core/audio.hpp"
#include "game/core/graphics.hpp"
#include "game/core/resources.hpp"
#include "game/core/state.hpp"

#include <filesystem>
#include <memory>

namespace game {

class Application {
public:
    explicit Application(const std::filesystem::path& executable);
    [[nodiscard]] int run();
    [[nodiscard]] int capture_ui(const std::filesystem::path& output_directory);

private:
    [[nodiscard]] bool initialise();
    void change_state(StateId next);

    Assets assets_;
    Graphics graphics_;
    Audio audio_;
    Resources resources_;
    bool sound_enabled_ = true;
    int last_score_ = 0;
    int high_score_ = 0;
    ShipType selected_ship_ = ShipType::laser;
    bool running_ = true;
    std::unique_ptr<State> state_;
};

}  // namespace game
