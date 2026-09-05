#include "game/states/menu_state.hpp"

#include "game/core/config.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>

namespace game {

namespace {

constexpr std::array<SDL_Rect, 3> hit_areas{{
    {225, 280, 350, 56},
    {225, 352, 350, 56},
    {225, 424, 350, 56},
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

MenuState::MenuState(Context context) : context_(context), ui_(context.graphics) {
    context_.graphics.show_cursor(true);
    if (!context_.audio.available()) {
        context_.sound_enabled = false;
    }
    context_.audio.play_music(context_.resources.background_music);
    if (!context_.sound_enabled) {
        context_.audio.pause_music();
    }
}

StateId MenuState::handle_event(const SDL_Event& event) {
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
            return StateId::quit;
        case SDLK_UP:
            do {
                selected_ = (selected_ + hit_areas.size() - 1) % hit_areas.size();
            } while (selected_ == 1 && !context_.audio.available());
            break;
        case SDLK_DOWN:
            do {
                selected_ = (selected_ + 1) % hit_areas.size();
            } while (selected_ == 1 && !context_.audio.available());
            break;
        case SDLK_RETURN:
        case SDLK_SPACE:
            return activate(selected_);
        default:
            break;
    }
    return StateId::none;
}

StateId MenuState::update(float delta_time) {
    elapsed_ += delta_time;
    const float blend = 1.0F - std::exp(-12.0F * delta_time);
    for (std::size_t i = 0; i < option_focus_.size(); ++i) {
        const float target = i == selected_ ? 1.0F : 0.0F;
        option_focus_[i] += (target - option_focus_[i]) * blend;
    }
    return StateId::none;
}

void MenuState::render(float interpolation) {
    (void)interpolation;
    context_.graphics.clear(context_.resources.menu);

    const float pulse = 0.5F + 0.5F * std::sin(elapsed_ * 2.4F);
    const float accent_width = 54.0F + pulse * 14.0F;

    ui_.panel({175.0F, 58.0F, 450.0F, 466.0F});
    ui_.text("ASTEROIDS", 400.0F, 98.0F, 6.0F, colors::white, TextAlign::center);
    ui_.text(
        "ORBITAL DEFENSE", 400.0F, 158.0F, 2.0F, colors::cyan,
        TextAlign::center);
    ui_.text(
        "PROTECT THE HOMEWORLD", 400.0F, 205.0F, 1.5F, colors::muted,
        TextAlign::center);
    SDL_Color title_accent = colors::cyan;
    title_accent.a = static_cast<Uint8>(90.0F + pulse * 80.0F);
    context_.graphics.fill_rect(
        {400.0F - accent_width, 184.0F, accent_width * 2.0F, 1.0F},
        title_accent);

    const std::string_view sound_label = !context_.audio.available()
        ? "SOUND UNAVAILABLE"
        : context_.sound_enabled ? "SOUND ON" : "SOUND OFF";
    ui_.button(as_float_rect(hit_areas[0]), "PLAY", option_focus_[0]);
    ui_.button(
        as_float_rect(hit_areas[1]), sound_label, option_focus_[1],
        context_.audio.available());
    ui_.button(as_float_rect(hit_areas[2]), "QUIT", option_focus_[2]);

    const std::string best_score = "BEST " + std::to_string(context_.high_score);
    ui_.text(
        best_score, 400.0F, 500.0F, 1.5F, colors::magenta, TextAlign::center);
    ui_.text(
        "ARROWS NAVIGATE  ENTER SELECT", 400.0F, 558.0F, 1.5F,
        colors::muted, TextAlign::center);
}

StateId MenuState::activate(std::size_t option) {
    if (option == 0) {
        return StateId::playing;
    }
    if (option == 1) {
        if (context_.audio.available()) {
            context_.sound_enabled = !context_.sound_enabled;
            if (context_.sound_enabled) {
                context_.audio.resume_music();
            } else {
                context_.audio.pause_music();
            }
        }
        return StateId::none;
    }
    return StateId::quit;
}

}  // namespace game
