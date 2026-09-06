#include "game/states/game_over_state.hpp"

#include <array>
#include <string>

namespace game {

namespace {

constexpr std::array<SDL_Rect, 3> hit_areas{{
    {225, 320, 350, 56},
    {225, 392, 350, 56},
    {225, 464, 350, 56},
}};

SDL_FRect as_float_rect(const SDL_Rect& rectangle) {
    return {
        static_cast<float>(rectangle.x),
        static_cast<float>(rectangle.y),
        static_cast<float>(rectangle.w),
        static_cast<float>(rectangle.h),
    };
}

}  // namespace

GameOverState::GameOverState(Context context)
    : context_(context), ui_(context.graphics) {
    context_.graphics.show_cursor(true);
    context_.audio.play_music(context_.resources.death_music, 0);
}

StateId GameOverState::handle_event(const SDL_Event& event) {
    if (event.type == SDL_QUIT) {
        return StateId::quit;
    }
    if (event.type == SDL_MOUSEMOTION) {
        mouse_ = {event.motion.x, event.motion.y};
        for (std::size_t i = 0; i < hit_areas.size(); ++i) {
            if (SDL_PointInRect(&mouse_, &hit_areas[i])) {
                selected_ = i;
                break;
            }
        }
    }
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        const SDL_Point point{event.button.x, event.button.y};
        for (std::size_t i = 0; i < hit_areas.size(); ++i) {
            if (SDL_PointInRect(&point, &hit_areas[i])) {
                return activate(i);
            }
        }
    }
    if (event.type != SDL_KEYDOWN || event.key.repeat) {
        return StateId::none;
    }
    switch (event.key.keysym.sym) {
        case SDLK_ESCAPE:
            return StateId::menu;
        case SDLK_UP:
            selected_ = (selected_ + hit_areas.size() - 1) % hit_areas.size();
            break;
        case SDLK_DOWN:
            selected_ = (selected_ + 1) % hit_areas.size();
            break;
        case SDLK_RETURN:
        case SDLK_SPACE:
            return activate(selected_);
        case SDLK_q:
            return StateId::quit;
        default:
            break;
    }
    return StateId::none;
}

StateId GameOverState::update(float delta_time) {
    elapsed_ += delta_time;
    return StateId::none;
}

void GameOverState::render(float interpolation) {
    (void)interpolation;
    context_.graphics.clear(context_.resources.menu);
    context_.graphics.fill_rect({0.0F, 0.0F, 800.0F, 600.0F}, colors::overlay);
    ui_.starfield(elapsed_);
    ui_.panel({175.0F, 55.0F, 450.0F, 500.0F});

    ui_.text("MISSION OVER", 400.0F, 95.0F, 5.0F, colors::white, TextAlign::center);
    ui_.text("FLIGHT RECORDER / SIGNAL LOST", 400.0F, 151.0F, 1.0F,
        colors::coral, TextAlign::center);
    ui_.radar(115.0F, 300.0F, 40.0F, elapsed_, colors::coral);
    ui_.radar(685.0F, 300.0F, 40.0F, elapsed_, colors::coral);
    ui_.text(
        "SCORE " + std::to_string(context_.last_score), 400.0F, 188.0F,
        2.5F, colors::cyan, TextAlign::center);
    ui_.text(
        "BEST " + std::to_string(context_.high_score), 400.0F, 232.0F,
        2.0F, colors::magenta, TextAlign::center);

    ui_.button(as_float_rect(hit_areas[0]), "REDEPLOY", selected_ == 0);
    ui_.button(as_float_rect(hit_areas[1]), "MAIN MENU", selected_ == 1);
    ui_.button(as_float_rect(hit_areas[2]), "QUIT", selected_ == 2);
}

StateId GameOverState::activate(std::size_t option) const {
    if (option == 0) {
        return StateId::playing;
    }
    if (option == 1) {
        return StateId::menu;
    }
    return StateId::quit;
}

}  // namespace game
