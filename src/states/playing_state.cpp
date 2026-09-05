#include "game/states/playing_state.hpp"

#include "game/core/config.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

namespace game {

namespace {

constexpr float fire_interval = 0.16F;
constexpr std::array<SDL_Rect, 3> pause_hit_areas{{
    {240, 280, 320, 48},
    {240, 342, 320, 48},
    {240, 404, 320, 48},
}};

int asteroid_points(AsteroidSize size) {
    if (size == AsteroidSize::large) {
        return 20;
    }
    if (size == AsteroidSize::medium) {
        return 50;
    }
    return 100;
}

int asteroid_pixels(AsteroidSize size) {
    if (size == AsteroidSize::large) {
        return 70;
    }
    if (size == AsteroidSize::medium) {
        return 40;
    }
    return 20;
}

SDL_FRect centered_rect(Vector2 position, float size) {
    return {
        position.x + config::screen_width / 2.0F - size / 2.0F,
        position.y + config::screen_height / 2.0F - size / 2.0F,
        size,
        size,
    };
}

SDL_FRect expanded(SDL_FRect rectangle, float amount) {
    return {
        rectangle.x - amount,
        rectangle.y - amount,
        rectangle.w + amount * 2.0F,
        rectangle.h + amount * 2.0F,
    };
}

SDL_FRect offset(SDL_FRect rectangle, float x, float y) {
    rectangle.x += x;
    rectangle.y += y;
    return rectangle;
}

SDL_Color faded(SDL_Color color, float opacity) {
    color.a = static_cast<std::uint8_t>(
        static_cast<float>(color.a) * std::clamp(opacity, 0.0F, 1.0F));
    return color;
}

}  // namespace

PlayingState::PlayingState(Context context)
    : context_(context), ui_(context.graphics) {
    context_.graphics.show_cursor(false);
}

StateId PlayingState::handle_event(const SDL_Event& event) {
    if (event.type == SDL_QUIT) {
        return StateId::quit;
    }
    if (event.type == SDL_WINDOWEVENT &&
        event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
        thrusting_ = false;
        turning_left_ = false;
        turning_right_ = false;
        firing_ = false;
        paused_ = true;
        context_.graphics.show_cursor(true);
        return StateId::none;
    }

    if (paused_ && event.type == SDL_MOUSEMOTION) {
        const SDL_Point mouse{event.motion.x, event.motion.y};
        for (std::size_t i = 0; i < pause_hit_areas.size(); ++i) {
            if (SDL_PointInRect(&mouse, &pause_hit_areas[i])) {
                pause_selected_ = i;
                break;
            }
        }
        return StateId::none;
    }
    if (paused_ && event.type == SDL_MOUSEBUTTONDOWN) {
        const SDL_Point mouse{event.button.x, event.button.y};
        for (std::size_t i = 0; i < pause_hit_areas.size(); ++i) {
            if (SDL_PointInRect(&mouse, &pause_hit_areas[i])) {
                return pause_action(i);
            }
        }
        return StateId::none;
    }
    if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP) {
        return StateId::none;
    }

    const bool pressed = event.type == SDL_KEYDOWN;
    if (pressed && !event.key.repeat &&
        (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_p)) {
        paused_ = !paused_;
        thrusting_ = false;
        turning_left_ = false;
        turning_right_ = false;
        firing_ = false;
        context_.graphics.show_cursor(paused_);
        return StateId::none;
    }
    if (paused_) {
        if (pressed && !event.key.repeat) {
            if (event.key.keysym.sym == SDLK_RETURN) {
                return pause_action(pause_selected_);
            } else if (event.key.keysym.sym == SDLK_UP) {
                pause_selected_ =
                    (pause_selected_ + pause_hit_areas.size() - 1) %
                    pause_hit_areas.size();
            } else if (event.key.keysym.sym == SDLK_DOWN) {
                pause_selected_ = (pause_selected_ + 1) % pause_hit_areas.size();
            } else if (event.key.keysym.sym == SDLK_m) {
                return StateId::menu;
            } else if (event.key.keysym.sym == SDLK_q) {
                return StateId::quit;
            }
        }
        return StateId::none;
    }

    switch (event.key.keysym.sym) {
        case SDLK_ESCAPE:
        case SDLK_p:
            break;
        case SDLK_UP:
            thrusting_ = pressed;
            break;
        case SDLK_LEFT:
            turning_left_ = pressed;
            break;
        case SDLK_RIGHT:
            turning_right_ = pressed;
            break;
        case SDLK_SPACE:
            firing_ = pressed;
            break;
        default:
            break;
    }
    return StateId::none;
}

StateId PlayingState::update(float delta_time) {
    elapsed_ += delta_time;
    const float pause_target = paused_ ? 1.0F : 0.0F;
    const float pause_blend = 1.0F - std::exp(-11.0F * delta_time);
    pause_amount_ += (pause_target - pause_amount_) * pause_blend;
    if (std::abs(pause_amount_ - pause_target) < 0.001F) {
        pause_amount_ = pause_target;
    }

    if (paused_) {
        return StateId::none;
    }

    update_particles(delta_time);
    screen_shake_ = std::max(0.0F, screen_shake_ - delta_time * 18.0F);

    const PlayerInput input{
        static_cast<float>(turning_right_) - static_cast<float>(turning_left_),
        thrusting_,
    };
    player_.update(delta_time, input);

    if (thrusting_) {
        exhaust_timer_ -= delta_time;
        while (exhaust_timer_ <= 0.0F) {
            spawn_exhaust();
            exhaust_timer_ += 0.024F;
        }
    } else {
        exhaust_timer_ = 0.0F;
    }

    fire_cooldown_ = std::max(0.0F, fire_cooldown_ - delta_time);
    if (firing_ && fire_cooldown_ <= 0.0F && player_.shoot()) {
        spawn_muzzle_flash();
        screen_shake_ = std::max(screen_shake_, 0.9F);
        context_.audio.play_sound(context_.resources.fire_sound);
        fire_cooldown_ = fire_interval;
    }

    asteroid_field_.update(delta_time);
    invulnerability_ = std::max(0.0F, invulnerability_ - delta_time);
    hint_timer_ = std::max(0.0F, hint_timer_ - delta_time);
    wave_banner_timer_ = std::max(0.0F, wave_banner_timer_ - delta_time);

    if (invulnerability_ <= 0.0F &&
        asteroid_field_.collision_index(player_.position(), Player::hit_radius) != -1) {
        spawn_impact(player_.position(), 26.0F);
        screen_shake_ = std::max(screen_shake_, 6.0F);
        if (player_.take_hit()) {
            context_.last_score = score_;
            context_.high_score = std::max(context_.high_score, score_);
            return StateId::game_over;
        }
        invulnerability_ = 1.0F;
    }

    for (Bullet& bullet : player_.bullets()) {
        if (!bullet.alive) {
            continue;
        }
        const int index = asteroid_field_.collision_index_on_segment(
            bullet.previous_position, bullet.position, 1.0F);
        if (index == -1) {
            continue;
        }
        score_ += asteroid_points(
            asteroid_field_.asteroids()[static_cast<std::size_t>(index)].size);
        const Asteroid& hit =
            asteroid_field_.asteroids()[static_cast<std::size_t>(index)];
        spawn_impact(hit.position, hit.radius);
        bullet.alive = false;
        asteroid_field_.destroy_and_split(static_cast<std::size_t>(index));
        context_.audio.play_sound(context_.resources.impact_sound);
    }
    player_.cull_bullets();

    if (asteroid_field_.active_count() == 0) {
        ++wave_;
        const std::size_t asteroid_count = std::min<std::size_t>(3 + wave_ / 2, 6);
        const float speed_scale = 1.0F + static_cast<float>(wave_ - 1) * 0.1F;
        asteroid_field_.reset(asteroid_count, speed_scale);
        wave_banner_timer_ = 1.6F;
    }
    return StateId::none;
}

void PlayingState::render(float interpolation) {
    context_.graphics.clear(context_.resources.background);
    context_.graphics.fill_rect(
        {0.0F, 0.0F, 800.0F, 600.0F}, colors::background_veil);

    const Vector2 camera{
        std::sin(elapsed_ * 91.0F) * screen_shake_,
        std::cos(elapsed_ * 73.0F) * screen_shake_ * 0.72F,
    };

    for (const Particle& particle : particles_) {
        if (particle.life <= 0.0F) {
            continue;
        }
        const float progress = particle.life / particle.duration;
        const float size = particle.size * (0.45F + progress * 0.55F);
        SDL_Color color = particle.color;
        color.a = static_cast<std::uint8_t>(
            static_cast<float>(color.a) * progress * progress);
        context_.graphics.fill_rect(
            offset(centered_rect(particle.position, size), camera.x, camera.y), color);
    }

    for (const Shockwave& shockwave : shockwaves_) {
        if (shockwave.life <= 0.0F) {
            continue;
        }
        const float remaining = shockwave.life / shockwave.duration;
        const float progress = 1.0F - remaining;
        const float diameter = shockwave.radius * 2.0F * progress;
        SDL_Color color = shockwave.color;
        color.a = static_cast<std::uint8_t>(
            static_cast<float>(color.a) * remaining * remaining);
        context_.graphics.stroke_rect(
            offset(centered_rect(shockwave.position, diameter), camera.x, camera.y),
            color,
            std::max(1.0F, 3.0F * remaining));
    }

    const float invulnerability_pulse = 0.5F + 0.5F * std::sin(elapsed_ * 20.0F);
    const std::uint8_t ship_alpha = invulnerability_ > 0.0F
        ? static_cast<std::uint8_t>(85.0F + invulnerability_pulse * 170.0F)
        : 255;
    const SDL_FRect ship = offset(
        centered_rect(player_.render_position(interpolation), 70.0F),
        camera.x,
        camera.y);
    const float engine_pulse = thrusting_
        ? 0.5F + 0.5F * std::sin(elapsed_ * 28.0F)
        : 0.0F;
    context_.graphics.set_tint(context_.resources.ship, colors::ship_shadow);
    context_.graphics.set_alpha(context_.resources.ship, 82);
    context_.graphics.draw_rotated(
        context_.resources.ship, offset(ship, 3.0F, 4.0F),
        player_.render_angle(interpolation));

    context_.graphics.set_tint(
        context_.resources.ship, colors::cyan);
    context_.graphics.set_alpha(
        context_.resources.ship,
        static_cast<std::uint8_t>(35.0F + engine_pulse * 45.0F));
    context_.graphics.draw_rotated(
        context_.resources.ship, expanded(ship, 3.0F + engine_pulse * 2.0F),
        player_.render_angle(interpolation));

    context_.graphics.set_tint(context_.resources.ship, SDL_Color{255, 255, 255, 255});
    context_.graphics.set_alpha(context_.resources.ship, ship_alpha);
    context_.graphics.draw_rotated(
        context_.resources.ship, ship, player_.render_angle(interpolation));
    context_.graphics.set_alpha(context_.resources.ship, 255);

    const auto& asteroids = asteroid_field_.asteroids();
    for (std::size_t index = 0; index < asteroids.size(); ++index) {
        const Asteroid& asteroid = asteroids[index];
        if (!asteroid.alive) {
            continue;
        }
        const float float_phase =
            elapsed_ * 1.8F + static_cast<float>(index) * 1.37F;
        const float lift = std::sin(float_phase) * 1.5F;
        const float scale_pulse = 1.0F + std::sin(float_phase * 0.7F) * 0.018F;
        const float base_size = static_cast<float>(asteroid_pixels(asteroid.size));
        const SDL_FRect destination = centered_rect(
            asteroid.render_position(interpolation),
            base_size * scale_pulse);
        const SDL_FRect shaken_destination =
            offset(destination, camera.x, camera.y);

        const float rim_pulse = 0.5F + 0.5F * std::sin(float_phase * 1.25F);
        context_.graphics.set_tint(context_.resources.asteroid, colors::amber);
        context_.graphics.set_alpha(
            context_.resources.asteroid,
            static_cast<std::uint8_t>(28.0F + rim_pulse * 30.0F));
        context_.graphics.draw_rotated(
            context_.resources.asteroid,
            expanded(offset(shaken_destination, 0.0F, lift), 2.0F + rim_pulse),
            asteroid.render_angle(interpolation));

        context_.graphics.set_tint(context_.resources.asteroid, colors::asteroid_shadow);
        context_.graphics.set_alpha(context_.resources.asteroid, 92);
        context_.graphics.draw_rotated(
            context_.resources.asteroid,
            offset(shaken_destination, 3.0F, 4.0F + lift),
            asteroid.render_angle(interpolation));

        context_.graphics.set_tint(
            context_.resources.asteroid, SDL_Color{255, 255, 255, 255});
        context_.graphics.set_alpha(context_.resources.asteroid, 255);
        context_.graphics.draw_rotated(
            context_.resources.asteroid, offset(shaken_destination, 0.0F, lift),
            asteroid.render_angle(interpolation));
    }

    for (const Bullet& bullet : player_.bullets()) {
        if (bullet.alive) {
            const SDL_FRect destination =
                offset(
                    centered_rect(bullet.render_position(interpolation), 10.0F),
                    camera.x,
                    camera.y);
            context_.graphics.set_tint(
                context_.resources.bullet, colors::cyan);
            context_.graphics.set_alpha(context_.resources.bullet, 75);
            context_.graphics.draw(
                context_.resources.bullet, expanded(destination, 3.0F));
            context_.graphics.set_tint(
                context_.resources.bullet, SDL_Color{255, 255, 255, 255});
            context_.graphics.set_alpha(context_.resources.bullet, 255);
            context_.graphics.draw(context_.resources.bullet, destination);
        }
    }

    ui_.panel({12.0F, 12.0F, 776.0F, 56.0F}, false);
    ui_.text(
        "SCORE " + std::to_string(score_), 28.0F, 31.0F, 2.0F, colors::white);
    ui_.text(
        "WAVE " + std::to_string(wave_), 380.0F, 31.0F, 2.0F,
        colors::cyan, TextAlign::center);
    ui_.text(
        "LEFT " + std::to_string(asteroid_field_.active_count()),
        620.0F, 31.0F, 2.0F, colors::muted, TextAlign::right);

    for (int i = 0; i < player_.lives(); ++i) {
        const SDL_FRect icon{650.0F + static_cast<float>(i * 38), 25.0F, 30.0F, 30.0F};
        context_.graphics.draw(context_.resources.ship, icon);
    }

    if (hint_timer_ > 0.0F && !paused_) {
        const float hint_opacity = std::min(1.0F, hint_timer_ / 0.75F);
        ui_.panel({175.0F, 545.0F, 450.0F, 36.0F}, false, hint_opacity);
        ui_.text(
            "ARROWS MOVE  SPACE FIRE  ESC PAUSE", 400.0F, 556.0F, 1.5F,
            faded(colors::muted, hint_opacity), TextAlign::center);
    }

    if (wave_banner_timer_ > 0.0F && !paused_) {
        const float age = 1.6F - wave_banner_timer_;
        const float banner_opacity =
            std::min(1.0F, age / 0.2F) * std::min(1.0F, wave_banner_timer_ / 0.35F);
        const float banner_y = 102.0F - (1.0F - std::min(1.0F, age / 0.3F)) * 10.0F;
        ui_.panel({290.0F, banner_y, 220.0F, 58.0F}, false, banner_opacity);
        ui_.text(
            "WAVE " + std::to_string(wave_), 400.0F, banner_y + 20.0F, 2.5F,
            faded(colors::cyan, banner_opacity), TextAlign::center);
    }

    if (paused_ || pause_amount_ > 0.001F) {
        const float panel_offset = (1.0F - pause_amount_) * 12.0F;
        context_.graphics.fill_rect(
            {0.0F, 0.0F, 800.0F, 600.0F}, faded(colors::overlay, pause_amount_));
        ui_.panel(
            {190.0F, 112.0F + panel_offset, 420.0F, 376.0F}, true,
            pause_amount_);
        ui_.text(
            "PAUSED", 400.0F, 148.0F + panel_offset, 5.0F,
            faded(colors::white, pause_amount_), TextAlign::center);
        ui_.text(
            "MISSION STANDBY", 400.0F, 212.0F + panel_offset, 1.5F,
            faded(colors::cyan, pause_amount_),
            TextAlign::center);
        ui_.button(
            {240.0F, 280.0F + panel_offset, 320.0F, 48.0F}, "RESUME",
            pause_selected_ == 0 ? pause_amount_ : 0.0F, true, pause_amount_);
        ui_.button(
            {240.0F, 342.0F + panel_offset, 320.0F, 48.0F}, "MAIN MENU",
            pause_selected_ == 1 ? pause_amount_ : 0.0F, true, pause_amount_);
        ui_.button(
            {240.0F, 404.0F + panel_offset, 320.0F, 48.0F}, "QUIT",
            pause_selected_ == 2 ? pause_amount_ : 0.0F, true, pause_amount_);
    }
}

void PlayingState::update_particles(float delta_time) {
    for (Particle& particle : particles_) {
        if (particle.life <= 0.0F) {
            continue;
        }
        particle.life = std::max(0.0F, particle.life - delta_time);
        particle.position += particle.velocity * delta_time;
        particle.velocity *= std::max(0.0F, 1.0F - delta_time * 2.4F);
    }
    for (Shockwave& shockwave : shockwaves_) {
        shockwave.life = std::max(0.0F, shockwave.life - delta_time);
    }
}

void PlayingState::spawn_exhaust() {
    const Vector2 backward = player_.direction() * -1.0F;
    const Vector2 origin = player_.position() + backward * 27.0F;
    for (int particle = 0; particle < 3; ++particle) {
        const float spread = (random_unit() - 0.5F) * 30.0F;
        const float speed = 58.0F + random_unit() * 58.0F;
        const Vector2 jitter{
            (random_unit() - 0.5F) * 5.0F,
            (random_unit() - 0.5F) * 5.0F,
        };
        const SDL_Color color = particle == 0
            ? SDL_Color{238, 250, 255, 245}
            : particle == 1 ? SDL_Color{104, 232, 255, 225}
                            : SDL_Color{255, 125, 61, 215};
        emit_particle(
            origin + jitter,
            backward.rotated(spread) * speed,
            0.28F + random_unit() * 0.24F,
            3.0F + random_unit() * 4.5F,
            color);
    }
}

void PlayingState::spawn_muzzle_flash() {
    const Vector2 forward = player_.direction();
    const Vector2 origin = player_.position() + forward * 28.0F;
    for (int particle = 0; particle < 9; ++particle) {
        const float spread = (random_unit() - 0.5F) * 54.0F;
        const float speed = 90.0F + random_unit() * 120.0F;
        const SDL_Color color = particle % 3 == 0
            ? SDL_Color{255, 252, 225, 255}
            : particle % 3 == 1 ? SDL_Color{104, 232, 255, 245}
                                : SDL_Color{255, 184, 77, 240};
        emit_particle(
            origin,
            forward.rotated(spread) * speed,
            0.07F + random_unit() * 0.1F,
            3.0F + random_unit() * 5.0F,
            color);
    }
    emit_shockwave(origin, 0.12F, 15.0F, SDL_Color{184, 246, 255, 220});
}

void PlayingState::spawn_impact(Vector2 position, float radius) {
    const int count = 24 + static_cast<int>(radius * 0.6F);
    for (int particle = 0; particle < count; ++particle) {
        const float angle =
            360.0F * static_cast<float>(particle) / static_cast<float>(count) +
            (random_unit() - 0.5F) * 24.0F;
        const float speed = 70.0F + random_unit() * (110.0F + radius * 1.8F);
        const SDL_Color color = particle % 4 == 0
            ? SDL_Color{238, 250, 255, 255}
            : particle % 4 == 1 ? SDL_Color{255, 184, 77, 248}
            : particle % 4 == 2 ? SDL_Color{104, 232, 255, 240}
                                : SDL_Color{255, 92, 112, 230};
        emit_particle(
            position,
            Vector2{0.0F, -1.0F}.rotated(angle) * speed,
            0.42F + random_unit() * 0.42F,
            2.5F + random_unit() * 6.5F,
            color);
    }
    emit_shockwave(
        position, 0.36F, radius * 1.75F, SDL_Color{255, 184, 77, 245});
    emit_shockwave(
        position, 0.52F, radius * 2.45F, SDL_Color{104, 232, 255, 215});
    screen_shake_ = std::max(screen_shake_, 2.8F + radius * 0.11F);
}

void PlayingState::emit_particle(
    Vector2 position,
    Vector2 velocity,
    float duration,
    float size,
    SDL_Color color) {
    for (Particle& particle : particles_) {
        if (particle.life > 0.0F) {
            continue;
        }
        particle = {position, velocity, color, duration, duration, size};
        return;
    }
}

void PlayingState::emit_shockwave(
    Vector2 position,
    float duration,
    float radius,
    SDL_Color color) {
    for (Shockwave& shockwave : shockwaves_) {
        if (shockwave.life > 0.0F) {
            continue;
        }
        shockwave = {position, color, duration, duration, radius};
        return;
    }
}

float PlayingState::random_unit() {
    particle_seed_ = particle_seed_ * 1664525U + 1013904223U;
    return static_cast<float>((particle_seed_ >> 8U) & 0x00FFFFFFU) /
        static_cast<float>(0x01000000U);
}

StateId PlayingState::pause_action(std::size_t option) {
    if (option == 0) {
        paused_ = false;
        context_.graphics.show_cursor(false);
        return StateId::none;
    }
    if (option == 1) {
        return StateId::menu;
    }
    return StateId::quit;
}

}  // namespace game
