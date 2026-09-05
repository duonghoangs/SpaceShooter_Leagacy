#include "game/core/application.hpp"

#include "game/core/config.hpp"
#include "game/core/context.hpp"
#include "game/states/game_over_state.hpp"
#include "game/states/menu_state.hpp"
#include "game/states/playing_state.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace game {

Application::Application(const std::filesystem::path& executable)
    : assets_(executable) {}

int Application::run() {
    if (!initialise()) {
        return EXIT_FAILURE;
    }

    change_state(StateId::menu);
    using Clock = std::chrono::steady_clock;
    constexpr std::chrono::duration<double> render_interval{
        1.0 / config::maximum_render_rate};
    auto previous = Clock::now();
    double accumulator = 0.0;

    while (running_) {
        const auto frame_started = Clock::now();
        const auto current = frame_started;
        const double frame_time = std::min(
            std::chrono::duration<double>(current - previous).count(),
            static_cast<double>(config::maximum_frame_time));
        previous = current;
        accumulator += frame_time;

        SDL_Event event;
        while (running_ && SDL_PollEvent(&event)) {
            const StateId next = state_->handle_event(event);
            if (next != StateId::none) {
                change_state(next);
            }
        }

        while (running_ && accumulator >= config::fixed_timestep) {
            const StateId next = state_->update(config::fixed_timestep);
            accumulator -= config::fixed_timestep;
            if (next != StateId::none) {
                change_state(next);
                accumulator = 0.0;
                break;
            }
        }

        if (running_) {
            const float interpolation = static_cast<float>(
                accumulator / static_cast<double>(config::fixed_timestep));
            state_->render(interpolation);
            graphics_.present();
            std::this_thread::sleep_until(frame_started + render_interval);
        }
    }
    return EXIT_SUCCESS;
}

int Application::capture_ui(const std::filesystem::path& output_directory) {
    if (!initialise()) {
        return EXIT_FAILURE;
    }
    std::error_code error;
    std::filesystem::create_directories(output_directory, error);
    if (error) {
        return EXIT_FAILURE;
    }

    const auto capture = [this, &output_directory](
                             StateId state, const std::filesystem::path& filename) {
        change_state(state);
        state_->render(1.0F);
        return graphics_.save_screenshot(output_directory / filename);
    };

    bool success = capture(StateId::menu, "menu.bmp");
    success = capture(StateId::playing, "gameplay.bmp") && success;

    SDL_Event pause_event{};
    pause_event.type = SDL_KEYDOWN;
    pause_event.key.keysym.sym = SDLK_p;
    state_->handle_event(pause_event);
    for (int frame = 0; frame < 30; ++frame) {
        state_->update(config::fixed_timestep);
    }
    state_->render(1.0F);
    success = graphics_.save_screenshot(output_directory / "pause.bmp") && success;

    last_score_ = 1250;
    high_score_ = 2400;
    success = capture(StateId::game_over, "game-over.bmp") && success;
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

bool Application::initialise() {
    if (!graphics_.initialise()) {
        return false;
    }
    (void)audio_.initialise();

    resources_.background =
        graphics_.load_texture(assets_.image("space-enhanced-v4.png"));
    resources_.menu =
        graphics_.load_texture(assets_.image("menu-enhanced-v2.png"));
    resources_.ship = graphics_.load_texture(assets_.image("blue-ship.png"));
    resources_.asteroid = graphics_.load_texture(assets_.image("asteroid.png"));
    resources_.bullet = graphics_.load_texture(assets_.image("blue-bullet.png"));

    if (!resources_.background || !resources_.menu || !resources_.ship ||
        !resources_.asteroid || !resources_.bullet) {
        std::cerr << "Required image asset missing under " << assets_.root() << '\n';
        return false;
    }

    resources_.background_music = audio_.load_music(assets_.audio("background.mp3"));
    resources_.death_music = audio_.load_music(assets_.audio("death.mp3"));
    resources_.fire_sound = audio_.load_sound(assets_.audio("fire.wav"));
    resources_.impact_sound = audio_.load_sound(assets_.audio("asteroid.wav"));
    return true;
}

void Application::change_state(StateId next) {
    if (next == StateId::none) {
        return;
    }
    if (next == StateId::quit) {
        running_ = false;
        state_.reset();
        return;
    }

    Context context{
        graphics_, audio_, resources_, sound_enabled_, last_score_, high_score_};
    if (next == StateId::menu) {
        state_ = std::make_unique<MenuState>(context);
    } else if (next == StateId::playing) {
        state_ = std::make_unique<PlayingState>(context);
    } else if (next == StateId::game_over) {
        state_ = std::make_unique<GameOverState>(context);
    }
}

}  // namespace game
