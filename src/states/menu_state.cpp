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
    {225, 360, 350, 50},
    {225, 422, 350, 50},
    {225, 484, 350, 50},
}};

constexpr std::array<SDL_Rect, ship_count> ship_hit_areas{{
    {140, 185, 160, 150},
    {320, 185, 160, 150},
    {500, 185, 160, 150},
}};

SDL_FRect as_float_rect(const SDL_Rect& rectangle) {
    return {
        static_cast<float>(rectangle.x),
        static_cast<float>(rectangle.y),
        static_cast<float>(rectangle.w),
        static_cast<float>(rectangle.h),
    };
}

SDL_Color type_color(ShipType type) {
    if (type == ShipType::laser) {
        return colors::cyan;
    }
    if (type == ShipType::twin) {
        return colors::violet;
    }
    return colors::amber;
}

std::string_view type_detail(ShipType type) {
    if (type == ShipType::laser) {
        return "15 LV BEAM";
    }
    if (type == ShipType::twin) {
        return "FIXED 2 SHOT";
    }
    return "SLOW HEAVY BLAST";
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
    ship_focus_.fill(0.0F);
    ship_focus_[ship_index(context_.selected_ship)] = 1.0F;
}

StateId MenuState::handle_event(const SDL_Event& event) {
    if (event.type == SDL_QUIT) {
        return StateId::quit;
    }
    if (event.type == SDL_MOUSEMOTION) {
        mouse_ = {event.motion.x, event.motion.y};
        for (std::size_t i = 0; i < ship_hit_areas.size(); ++i) {
            if (SDL_PointInRect(&mouse_, &ship_hit_areas[i])) {
                context_.selected_ship = static_cast<ShipType>(i);
                break;
            }
        }
        for (std::size_t i = 0; i < hit_areas.size(); ++i) {
            if (SDL_PointInRect(&mouse_, &hit_areas[i])) {
                selected_ = i;
                break;
            }
        }
    }
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        const SDL_Point point{event.button.x, event.button.y};
        for (std::size_t i = 0; i < ship_hit_areas.size(); ++i) {
            if (SDL_PointInRect(&point, &ship_hit_areas[i])) {
                context_.selected_ship = static_cast<ShipType>(i);
                return StateId::none;
            }
        }
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
        case SDLK_LEFT:
            context_.selected_ship = static_cast<ShipType>(
                (ship_index(context_.selected_ship) + ship_count - 1) % ship_count);
            break;
        case SDLK_RIGHT:
            context_.selected_ship = static_cast<ShipType>(
                (ship_index(context_.selected_ship) + 1) % ship_count);
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
    for (std::size_t i = 0; i < ship_focus_.size(); ++i) {
        const float target = i == ship_index(context_.selected_ship) ? 1.0F : 0.0F;
        ship_focus_[i] += (target - ship_focus_[i]) * blend;
    }
    return StateId::none;
}

void MenuState::render(float interpolation) {
    (void)interpolation;
    context_.graphics.clear(context_.resources.menu);
    context_.graphics.fill_rect({0, 0, 800, 600}, SDL_Color{2, 7, 18, 215});
    ui_.starfield(elapsed_);

    const float pulse = 0.5F + 0.5F * std::sin(elapsed_ * 2.4F);
    const float accent_width = 54.0F + pulse * 14.0F;

    ui_.text("FLIGHT COMMAND / 01", 28.0F, 20.0F, 1.0F, colors::muted);
    ui_.text("SYSTEM ONLINE", 772.0F, 20.0F, 1.0F, colors::cyan, TextAlign::right);
    ui_.text("SPACE SHOOTER", 400.0F, 55.0F, 4.0F, colors::white, TextAlign::center);
    ui_.text(
        "LEGACY / ORBITAL DEFENSE", 400.0F, 108.0F, 2.0F, colors::cyan,
        TextAlign::center);
    SDL_Color title_accent = colors::cyan;
    title_accent.a = static_cast<Uint8>(90.0F + pulse * 80.0F);
    context_.graphics.fill_rect(
        {400.0F - accent_width, 137.0F, accent_width * 2.0F, 1.0F},
        title_accent);
    ui_.text(
        "HANGAR / SELECT YOUR SHIP", 400.0F, 154.0F, 1.0F, colors::muted,
        TextAlign::center);

    for (std::size_t i = 0; i < ship_count; ++i) {
        const ShipType type = static_cast<ShipType>(i);
        const SDL_FRect card = as_float_rect(ship_hit_areas[i]);
        ui_.button(card, "", ship_focus_[i]);
        ui_.radar(card.x + card.w / 2.0F, card.y + 53.0F, 42.0F,
            elapsed_ + static_cast<float>(i) * 2.0F, type_color(type));
        const SDL_FRect ship{
            card.x + card.w / 2.0F - 43.0F,
            card.y + 8.0F + std::sin(elapsed_ * 2.0F + static_cast<float>(i)) * 4.0F,
            86.0F,
            86.0F,
        };
        context_.graphics.draw(context_.resources.ships[i], ship);
        ui_.text(
            ship_name(type), card.x + card.w / 2.0F, card.y + 103.0F, 2.0F,
            type_color(type), TextAlign::center);
        ui_.text(
            type_detail(type), card.x + card.w / 2.0F, card.y + 130.0F, 1.0F,
            colors::muted, TextAlign::center);
    }

    const std::string_view sound_label = !context_.audio.available()
        ? "SOUND UNAVAILABLE"
        : context_.sound_enabled ? "SOUND ON" : "SOUND OFF";
    ui_.button(as_float_rect(hit_areas[0]), "LAUNCH MISSION", option_focus_[0]);
    ui_.button(
        as_float_rect(hit_areas[1]), sound_label, option_focus_[1],
        context_.audio.available());
    ui_.button(as_float_rect(hit_areas[2]), "QUIT", option_focus_[2]);

    const std::string best_score = "BEST " + std::to_string(context_.high_score);
    ui_.text(
        best_score, 400.0F, 545.0F, 1.0F, colors::violet, TextAlign::center);
    ui_.text(
        "LEFT RIGHT SHIP  UP DOWN MENU", 400.0F, 582.0F, 1.0F,
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
