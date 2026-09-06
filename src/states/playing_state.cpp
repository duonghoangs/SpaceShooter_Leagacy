#include "game/states/playing_state.hpp"

#include "game/core/config.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

namespace game {

namespace {

constexpr std::array<SDL_Rect, 3> pause_hit_areas{{
    {240, 280, 320, 48},
    {240, 342, 320, 48},
    {240, 404, 320, 48},
}};
constexpr std::array<int, Player::laser_maximum_weapon_level - 1> upgrade_scores{
    250, 700, 1400, 2400, 3700, 5300, 7200,
    9400, 11900, 14700, 17800, 21200, 24900, 28900};
constexpr int experience_multiplier = 1;

int asteroid_points(AsteroidSize size) {
    if (size == AsteroidSize::large) {
        return 100 * experience_multiplier;
    }
    if (size == AsteroidSize::medium) {
        return 50 * experience_multiplier;
    }
    return 20 * experience_multiplier;
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

SDL_FRect centered_rect(Vector2 position, float width, float height) {
    return {
        position.x + config::screen_width / 2.0F - width / 2.0F,
        position.y + config::screen_height / 2.0F - height / 2.0F,
        width,
        height,
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

SDL_Color weapon_color(ShipType type) {
    if (type == ShipType::laser) {
        return colors::cyan;
    }
    if (type == ShipType::twin) {
        return colors::violet;
    }
    return colors::amber;
}

int next_upgrade_score(const Player& player) {
    if (player.weapon_level() >= player.maximum_weapon_level()) {
        return 0;
    }
    return upgrade_scores[static_cast<std::size_t>(player.weapon_level() - 1)];
}

const char* weapon_stage(const Player& player) {
    if (player.ship_type() != ShipType::laser) {
        return player.ship_type() == ShipType::twin ? "FIXED 2 SHOT" : "HEAVY BLAST";
    }
    if (player.weapon_level() <= 5) {
        return "DAMAGE";
    }
    if (player.weapon_level() <= 10) {
        return "BEAM SIZE";
    }
    return "RICOCHET";
}

const char* reload_label(ShipType type) {
    if (type == ShipType::laser) {
        return "RELOAD 0.16S";
    }
    if (type == ShipType::twin) {
        return "RELOAD 0.30S";
    }
    return "RELOAD 1.32S";
}

float ray_distance_to_edge(
    Vector2 origin, Vector2 direction, bool& hit_x, bool& hit_y) {
    constexpr float infinity = 1.0e9F;
    constexpr float epsilon = 0.0001F;
    const float half_width = config::screen_width / 2.0F;
    const float half_height = config::screen_height / 2.0F;

    float distance_x = infinity;
    if (direction.x > epsilon) {
        distance_x = (half_width - origin.x) / direction.x;
    } else if (direction.x < -epsilon) {
        distance_x = (-half_width - origin.x) / direction.x;
    }

    float distance_y = infinity;
    if (direction.y > epsilon) {
        distance_y = (half_height - origin.y) / direction.y;
    } else if (direction.y < -epsilon) {
        distance_y = (-half_height - origin.y) / direction.y;
    }

    const float distance = std::max(0.0F, std::min(distance_x, distance_y));
    hit_x = std::abs(distance_x - distance) < 0.01F;
    hit_y = std::abs(distance_y - distance) < 0.01F;
    return distance;
}

bool segment_hits_circle(
    Vector2 start, Vector2 end, Vector2 center, float radius) {
    const Vector2 segment = end - start;
    const float length_squared = segment.length_squared();
    float projection = 0.0F;
    if (length_squared > 0.0F) {
        projection = std::clamp(
            dot(center - start, segment) / length_squared, 0.0F, 1.0F);
    }
    const Vector2 difference = center - (start + segment * projection);
    return difference.length_squared() <= radius * radius;
}

}  // namespace

PlayingState::PlayingState(Context context)
    : context_(context), ui_(context.graphics), player_(context.selected_ship) {
    context_.graphics.show_cursor(false);
}

StateId PlayingState::handle_event(const SDL_Event& event) {
    if (event.type == SDL_QUIT) {
        return StateId::quit;
    }
    if (event.type == SDL_WINDOWEVENT &&
        event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
        thrusting_ = false;
        braking_ = false;
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
        braking_ = false;
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
        case SDLK_DOWN:
            braking_ = pressed;
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
    update_xp_popups(delta_time);
    screen_shake_ = std::max(0.0F, screen_shake_ - delta_time * 18.0F);
    survival_time_ += delta_time;
    upgrade_banner_timer_ = std::max(0.0F, upgrade_banner_timer_ - delta_time);
    laser_beam_.life = std::max(0.0F, laser_beam_.life - delta_time);

    const PlayerInput input{
        static_cast<float>(turning_right_) - static_cast<float>(turning_left_),
        thrusting_,
        braking_,
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
    bool fired = false;
    if (firing_ && fire_cooldown_ <= 0.0F) {
        if (player_.ship_type() == ShipType::laser) {
            fire_laser();
            fired = true;
        } else {
            fired = player_.shoot();
        }
    }
    if (fired) {
        spawn_muzzle_flash();
        screen_shake_ = std::max(screen_shake_, 0.9F);
        context_.audio.play_sound(context_.resources.fire_sound);
        fire_cooldown_ = player_.reload_time();
    }

    asteroid_field_.update(delta_time);
    invulnerability_ = std::max(0.0F, invulnerability_ - delta_time);
    hint_timer_ = std::max(0.0F, hint_timer_ - delta_time);
    wave_banner_timer_ = std::max(0.0F, wave_banner_timer_ - delta_time);

    if (invulnerability_ <= 0.0F &&
        asteroid_field_.collision_index(
            player_.position(), player_.collision_radius()) != -1) {
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
            bullet.previous_position, bullet.position, bullet.radius);
        if (index == -1) {
            continue;
        }
        const Asteroid& hit =
            asteroid_field_.asteroids()[static_cast<std::size_t>(index)];
        const Vector2 hit_position = hit.position;
        const float hit_radius = hit.radius;
        const AsteroidSize hit_size = hit.size;
        const AsteroidKind hit_kind = hit.kind;
        const bool destroyed = asteroid_field_.apply_damage(
            static_cast<std::size_t>(index), bullet.damage);
        bullet.alive = false;
        if (bullet.weapon == ShipType::cannon) {
            detonate_cannon(
                hit_position, bullet.radius, bullet.damage, index);
        }
        if (destroyed) {
            award_asteroid_xp(hit_position, hit_size, hit_kind);
            spawn_impact(hit_position, hit_radius);
            asteroid_field_.destroy_and_split(static_cast<std::size_t>(index));
            context_.audio.play_sound(context_.resources.impact_sound);
        } else {
            spawn_hit(hit_position, hit_radius);
        }
    }
    player_.cull_bullets();
    for (Bullet& bullet : player_.bullets()) {
        if (!bullet.explosion_pending) {
            continue;
        }
        detonate_cannon(bullet.position, bullet.radius, bullet.damage);
        bullet.explosion_pending = false;
    }

    if (!upgrade_pickup_.active &&
        player_.weapon_level() < player_.maximum_weapon_level() &&
        upgrade_xp_ >= next_upgrade_score(player_)) {
        spawn_upgrade_pickup();
    }
    update_upgrade_pickup();

    if (asteroid_field_.wave_count() == 0) {
        ++wave_;
        const std::size_t time_stage =
            static_cast<std::size_t>(survival_time_ / 45.0F);
        const std::size_t asteroid_count = std::min<std::size_t>(
            4 + wave_ / 2 + time_stage / 2, 10);
        const float speed_scale =
            1.0F + static_cast<float>(wave_ - 1) * 0.14F +
            static_cast<float>(time_stage) * 0.08F;
        const int health_bonus =
            static_cast<int>(std::min<std::size_t>((wave_ - 1) / 2 + time_stage / 2, 12));
        asteroid_field_.reset(asteroid_count, speed_scale, health_bonus);
        wave_banner_timer_ = 1.6F;
    }
    return StateId::none;
}

void PlayingState::prepare_encounter_capture() {
    // Advance only the field, so a visual fixture cannot kill the idle player.
    asteroid_field_ = AsteroidField(42);
    for (int frame = 0; frame < 690; ++frame) {
        asteroid_field_.update(config::fixed_timestep);
    }
    elapsed_ = 11.5F;
    hint_timer_ = 0.0F;
    wave_banner_timer_ = 0.0F;
    spawn_impact({110.0F, -100.0F}, 35.0F);
    update_particles(0.18F);
}

void PlayingState::render(float interpolation) {
    context_.graphics.clear(context_.resources.background);
    context_.graphics.fill_rect(
        {0.0F, 0.0F, 800.0F, 600.0F}, colors::background_veil);
    ui_.starfield(elapsed_);

    const Vector2 camera{
        std::sin(elapsed_ * 91.0F) * screen_shake_,
        std::cos(elapsed_ * 73.0F) * screen_shake_ * 0.72F,
    };
    const Graphics::Texture& active_ship =
        context_.resources.ships[ship_index(player_.ship_type())];

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

    for (const Debris& shard : debris_) {
        if (shard.life > 0.0F) {
            context_.graphics.asteroid(context_.resources.asteroid,
                {shard.position.x + 400.0F + camera.x, shard.position.y + 300.0F + camera.y},
                shard.radius, shard.angle, shard.seed,
                static_cast<std::uint8_t>(255.0F * std::min(1.0F, shard.life / 0.4F)));
        }
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

    if (upgrade_pickup_.active) {
        const float pulse = 0.5F + 0.5F * std::sin(elapsed_ * 6.0F);
        const SDL_FRect outer = offset(
            centered_rect(upgrade_pickup_.position, 30.0F + pulse * 6.0F),
            camera.x,
            camera.y);
        context_.graphics.fill_rect(outer, faded(colors::violet, 0.16F + pulse * 0.1F));
        context_.graphics.stroke_rect(outer, colors::cyan, 2.0F);
        const SDL_FRect core = expanded(outer, -9.0F - pulse * 1.5F);
        context_.graphics.fill_rect(core, colors::amber);
        context_.graphics.stroke_rect(core, colors::white, 1.0F);
        ui_.text(
            "P", outer.x + outer.w / 2.0F, outer.y + outer.h / 2.0F - 5.0F,
            1.5F, SDL_Color{4, 12, 31, 255}, TextAlign::center);
        ui_.text(
            "POWER", outer.x + outer.w / 2.0F, outer.y - 13.0F, 1.0F,
            colors::cyan, TextAlign::center);
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
    context_.graphics.set_tint(active_ship, colors::ship_shadow);
    context_.graphics.set_alpha(active_ship, 82);
    context_.graphics.draw_rotated(
        active_ship, offset(ship, 3.0F, 4.0F),
        player_.render_angle(interpolation));

    context_.graphics.set_tint(active_ship, weapon_color(player_.ship_type()));
    context_.graphics.set_alpha(
        active_ship,
        static_cast<std::uint8_t>(35.0F + engine_pulse * 45.0F));
    context_.graphics.draw_rotated(
        active_ship, expanded(ship, 3.0F + engine_pulse * 2.0F),
        player_.render_angle(interpolation));

    context_.graphics.set_tint(active_ship, SDL_Color{255, 255, 255, 255});
    context_.graphics.set_alpha(active_ship, ship_alpha);
    context_.graphics.draw_rotated(
        active_ship, ship, player_.render_angle(interpolation));
    context_.graphics.set_alpha(active_ship, 255);

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

        const SDL_FPoint center{
            shaken_destination.x + shaken_destination.w / 2.0F,
            shaken_destination.y + shaken_destination.h / 2.0F + lift};
        const bool is_meteor = asteroid.kind == AsteroidKind::meteor;
        const float body_angle = is_meteor
            ? std::atan2(asteroid.velocity.y, asteroid.velocity.x) * 57.2957795F
            : asteroid.render_angle(interpolation);
        if (is_meteor) {
            const Vector2 backward = asteroid.velocity.normalized() * -1.0F;
            const Vector2 normal{-backward.y, backward.x};
            // Each emission gets a new seeded lifetime and launch point.
            // A stylized heated leading face and wake, not a fluid simulation.
            const auto noise = [](std::uint32_t seed) {
                seed ^= seed >> 16; seed *= 0x7feb352dU;
                seed ^= seed >> 15; seed *= 0x846ca68bU;
                seed ^= seed >> 16;
                return static_cast<float>(seed >> 8) / 16777216.0F;
            };
            for (int particle = 0; particle < 96; ++particle) {
                const auto key = asteroid.shape_seed + static_cast<std::uint32_t>(particle) * 1013U;
                const float clock = elapsed_ * (1.2F + noise(key) * 1.7F) + noise(key + 1U);
                const auto cycle = static_cast<std::uint32_t>(clock);
                const float age = clock - static_cast<float>(cycle);
                const auto emission = key + cycle * 7919U;
                // Fewer embers survive far into the wake; dense near the hot head.
                const float lifetime = 0.22F + noise(emission + 7U) * 0.78F;
                if (age >= lifetime) {
                    continue;
                }
                const float spread = noise(emission + 2U) * 2.0F - 1.0F;
                const float launch = (0.12F + noise(emission + 6U) * 0.52F) * asteroid.radius;
                const float distance = -launch + age * (120.0F + noise(emission + 3U) * 95.0F);
                const float drift = spread * (asteroid.radius * 0.42F + age * 23.0F)
                    + std::sin(age * 8.0F + noise(emission + 4U) * 6.28F) * age * 7.0F;
                const float size = (1.0F - age) * (1.5F + noise(emission + 5U) * 4.0F);
                const float opacity = std::min(1.0F, age * 18.0F) *
                    std::min(1.0F, (lifetime - age) * 10.0F) * (1.0F - age);
                const SDL_FRect ember{
                    center.x + backward.x * distance + normal.x * drift - size / 2.0F,
                    center.y + backward.y * distance + normal.y * drift - size / 2.0F, size, size};
                const SDL_Color flame = age < 0.20F ? colors::white
                    : age < 0.48F ? colors::amber : colors::coral;
                context_.graphics.fill_rect(
                    {ember.x - size, ember.y - size, size * 3.0F, size * 3.0F},
                    faded(flame, opacity * 0.12F));
                context_.graphics.fill_rect(ember, faded(flame, opacity));
            }
            ui_.text("METEOR / BONUS", center.x, center.y + 44.0F,
                1.0F, colors::amber, TextAlign::center);
        }
        context_.graphics.asteroid(context_.resources.asteroid, center,
            asteroid.radius, body_angle, asteroid.shape_seed, 255, is_meteor);
        if (is_meteor) {
            const Vector2 forward = asteroid.velocity.normalized();
            const Vector2 normal{-forward.y, forward.x};
            for (int spot = 0; spot < 28; ++spot) {
                const float clock = elapsed_ * (1.7F + static_cast<float>(spot % 5) * 0.37F)
                    + static_cast<float>(spot) * 0.731F;
                const float age = clock - std::floor(clock);
                std::uint32_t seed = asteroid.shape_seed + static_cast<std::uint32_t>(spot) * 7919U
                    + static_cast<std::uint32_t>(clock) * 104729U;
                seed ^= seed >> 16; seed *= 0x7feb352dU; seed ^= seed >> 15;
                // Sample a patch of surface, not a shared arc along the rim.
                const float sample = static_cast<float>(seed & 65535U) / 65535.0F;
                const float along = 0.64F - sample * sample * 0.90F;
                seed ^= seed >> 16; seed *= 0x846ca68bU; seed ^= seed >> 16;
                const float across = (static_cast<float>(seed & 65535U) / 65535.0F - 0.5F) * 0.52F;
                const Vector2 position = forward * (along * asteroid.radius)
                    + normal * (across * asteroid.radius);
                const float pulse = std::sin(age * 3.14159265F);
                const float size = 1.0F + pulse * (1.0F + static_cast<float>(seed % 3U));
                const SDL_FRect patch{center.x + position.x - size / 2.0F,
                    center.y + position.y - size / 2.0F, size, size};
                context_.graphics.fill_rect({patch.x - 2.0F, patch.y - 2.0F,
                    size + 4.0F, size + 4.0F}, faded(colors::coral, pulse * 0.22F));
                context_.graphics.fill_rect(patch, faded(spot % 4 == 0 ? colors::white : colors::amber, pulse));
            }
        }
        if (asteroid.hit_flash > 0.0F) {
            context_.graphics.set_blend_mode(context_.resources.asteroid, SDL_BLENDMODE_ADD);
            context_.graphics.asteroid(context_.resources.asteroid, center,
                asteroid.radius, body_angle, asteroid.shape_seed,
                static_cast<std::uint8_t>(std::min(1.0F, asteroid.hit_flash / 0.1F) * 150.0F), is_meteor);
            context_.graphics.set_blend_mode(context_.resources.asteroid, SDL_BLENDMODE_BLEND);
        }

        if (asteroid.maximum_health > 1) {
            const float health_ratio =
                static_cast<float>(asteroid.health) /
                static_cast<float>(asteroid.maximum_health);
            const SDL_FRect health_back{
                shaken_destination.x,
                shaken_destination.y - 7.0F + lift,
                shaken_destination.w,
                3.0F,
            };
            context_.graphics.fill_rect(health_back, SDL_Color{2, 7, 18, 210});
            SDL_FRect health_fill = health_back;
            health_fill.w *= health_ratio;
            context_.graphics.fill_rect(
                health_fill, health_ratio > 0.5F ? colors::cyan : colors::coral);
        }
    }

    if (laser_beam_.life > 0.0F) {
        const float opacity = std::clamp(
            laser_beam_.life / std::max(0.045F, laser_beam_.duration * 0.35F),
            0.0F,
            1.0F);
        const Vector2 screen_offset{
            config::screen_width / 2.0F + camera.x,
            config::screen_height / 2.0F + camera.y,
        };
        for (std::size_t index = 0; index < laser_beam_.count; ++index) {
            const LaserSegment& segment = laser_beam_.segments[index];
            const Vector2 start = segment.start + screen_offset;
            const Vector2 end = segment.end + screen_offset;
            context_.graphics.draw_line(
                {start.x, start.y}, {end.x, end.y},
                faded(colors::cyan, opacity * 0.18F), laser_beam_.width + 12.0F);
            context_.graphics.draw_line(
                {start.x, start.y}, {end.x, end.y},
                faded(colors::cyan, opacity), laser_beam_.width + 3.0F);
            context_.graphics.draw_line(
                {start.x, start.y}, {end.x, end.y},
                faded(colors::white, opacity),
                std::max(1.5F, laser_beam_.width * 0.42F));
        }
    }

    for (const Bullet& bullet : player_.bullets()) {
        if (bullet.alive) {
            float width = 6.0F;
            float height = 18.0F;
            if (bullet.weapon == ShipType::twin) {
                width = 8.0F;
                height = 12.0F;
            } else {
                width = 17.0F;
                height = 17.0F;
            }
            const SDL_FRect destination = offset(
                centered_rect(
                    bullet.render_position(interpolation), width, height),
                camera.x,
                camera.y);
            const double projectile_angle =
                std::atan2(bullet.velocity.y, bullet.velocity.x) *
                    180.0 / 3.14159265358979323846 +
                90.0;
            const SDL_Color projectile_color = weapon_color(bullet.weapon);
            if (bullet.weapon == ShipType::cannon) {
                const float pulse = 0.5F + 0.5F *
                    std::sin(elapsed_ * 18.0F + bullet.travelled * 0.04F);
                context_.graphics.set_tint(context_.resources.bullet, colors::coral);
                context_.graphics.set_alpha(context_.resources.bullet, 48);
                context_.graphics.draw_rotated(
                    context_.resources.bullet,
                    expanded(destination, 8.0F + pulse * 4.0F), projectile_angle);
                context_.graphics.set_tint(context_.resources.bullet, colors::amber);
                context_.graphics.set_alpha(context_.resources.bullet, 105);
                context_.graphics.draw_rotated(
                    context_.resources.bullet,
                    expanded(destination, 4.0F + pulse * 2.0F), projectile_angle);
            }
            context_.graphics.set_tint(context_.resources.bullet, projectile_color);
            context_.graphics.set_alpha(context_.resources.bullet, 75);
            context_.graphics.draw_rotated(
                context_.resources.bullet, expanded(destination, 3.0F),
                projectile_angle);
            context_.graphics.set_tint(
                context_.resources.bullet, SDL_Color{255, 255, 255, 255});
            context_.graphics.set_alpha(context_.resources.bullet, 255);
            context_.graphics.draw_rotated(
                context_.resources.bullet, destination, projectile_angle);
        }
    }

    for (const XpPopup& popup : xp_popups_) {
        if (popup.life <= 0.0F) {
            continue;
        }
        const float opacity = std::clamp(popup.life / 0.3F, 0.0F, 1.0F);
        const float age = 1.0F - popup.life / popup.duration;
        ui_.text(
            "+" + std::to_string(popup.amount) + " XP",
            popup.position.x + config::screen_width / 2.0F + camera.x,
            popup.position.y + config::screen_height / 2.0F + camera.y,
            1.35F + age * 0.2F,
            faded(colors::amber, opacity),
            TextAlign::center);
    }

    ui_.panel({12.0F, 12.0F, 238.0F, 68.0F}, false);
    ui_.panel({260.0F, 12.0F, 368.0F, 68.0F}, false);
    ui_.panel({638.0F, 12.0F, 150.0F, 68.0F}, false);
    ui_.text("MISSION SCORE", 28.0F, 19.0F, 1.0F, colors::muted);
    ui_.text(
        "SCORE " + std::to_string(score_), 28.0F, 31.0F, 2.0F, colors::white);
    ui_.text(
        "WAVE " + std::to_string(wave_), 380.0F, 31.0F, 2.0F,
        colors::cyan, TextAlign::center);
    ui_.text(
        "HOSTILES " + std::to_string(asteroid_field_.active_count()),
        612.0F, 33.0F, 1.25F, colors::amber, TextAlign::right);

    ui_.text(
        std::string(ship_name(player_.ship_type())) + " LV " +
            std::to_string(player_.weapon_level()) + "  " + weapon_stage(player_),
        28.0F, 56.0F, 1.0F, weapon_color(player_.ship_type()));
    const int upgrade_score = next_upgrade_score(player_);
    ui_.text(
        upgrade_score > 0 ? "XP " + std::to_string(upgrade_xp_) + "/" + std::to_string(upgrade_score) : "MAX POWER",
        400.0F, 56.0F, 1.0F, colors::muted, TextAlign::center);
    ui_.text(
        reload_label(player_.ship_type()), 620.0F, 56.0F, 1.0F,
        colors::muted, TextAlign::right);

    for (int i = 0; i < player_.lives(); ++i) {
        const SDL_FRect icon{650.0F + static_cast<float>(i * 38), 25.0F, 30.0F, 30.0F};
        context_.graphics.draw(active_ship, icon);
    }
    ui_.text("HULL / " + std::to_string(player_.lives()), 713.0F, 62.0F,
        1.0F, player_.lives() <= 1 ? colors::coral : colors::cyan, TextAlign::center);
    if (upgrade_score > 0) {
        const float progress = std::clamp(static_cast<float>(upgrade_xp_) /
            static_cast<float>(upgrade_score), 0.0F, 1.0F);
        context_.graphics.fill_rect({28.0F, 72.0F, 204.0F, 2.0F}, colors::border);
        context_.graphics.fill_rect({28.0F, 72.0F, 204.0F * progress, 2.0F},
            weapon_color(player_.ship_type()));
    }

    if (meteor_bonus_timer_ > 0.0F && !paused_) {
        ui_.panel({220.0F, 175.0F, 360.0F, 42.0F}, false);
        ui_.text("METEOR BONUS +1500", 400.0F, 189.0F, 2.0F,
            colors::amber, TextAlign::center);
    }
    if (hint_timer_ > 0.0F && !paused_) {
        const float hint_opacity = std::min(1.0F, hint_timer_ / 0.75F);
        ui_.panel({75.0F, 545.0F, 650.0F, 36.0F}, false, hint_opacity);
        ui_.text(
            "UP THRUST  DOWN BRAKE  LEFT RIGHT TURN  SPACE FIRE", 400.0F,
            558.0F, 1.0F,
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

    if (upgrade_banner_timer_ > 0.0F && !paused_) {
        const float opacity = std::min(1.0F, upgrade_banner_timer_ / 0.3F);
        ui_.panel({250.0F, 482.0F, 300.0F, 48.0F}, false, opacity);
        ui_.text(
            "WEAPON LEVEL " + std::to_string(player_.weapon_level()),
            400.0F, 498.0F, 2.0F, faded(colors::amber, opacity),
            TextAlign::center);
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
    meteor_bonus_timer_ = std::max(0.0F, meteor_bonus_timer_ - delta_time);
    for (Debris& shard : debris_) {
        if (shard.life <= 0.0F) {
            continue;
        }
        shard.life = std::max(0.0F, shard.life - delta_time);
        shard.position += shard.velocity * delta_time;
        shard.angle += shard.spin * delta_time;
        shard.velocity *= std::max(0.0F, 1.0F - delta_time * 0.8F);
    }
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

void PlayingState::update_xp_popups(float delta_time) {
    for (XpPopup& popup : xp_popups_) {
        if (popup.life <= 0.0F) {
            continue;
        }
        popup.life = std::max(0.0F, popup.life - delta_time);
        popup.position.y -= 34.0F * delta_time;
    }
}

void PlayingState::award_asteroid_xp(Vector2 position, AsteroidSize size, AsteroidKind kind) {
    const int amount = kind == AsteroidKind::meteor ? 20 : asteroid_points(size);
    score_ += amount;
    upgrade_xp_ += amount;
    if (kind == AsteroidKind::meteor) {
        score_ += 1500;
        meteor_bonus_timer_ = 2.5F;
    }

    XpPopup* slot = &xp_popups_.front();
    for (XpPopup& popup : xp_popups_) {
        if (popup.life <= 0.0F) {
            slot = &popup;
            break;
        }
        if (popup.life < slot->life) {
            slot = &popup;
        }
    }
    constexpr float duration = 1.0F;
    *slot = {position, amount, duration, duration};
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
        SDL_Color color = colors::cyan;
        if (player_.ship_type() == ShipType::twin) {
            color = particle == 0 ? colors::white : colors::violet;
        } else if (player_.ship_type() == ShipType::cannon) {
            color = particle == 0 ? colors::white :
                particle == 1 ? colors::amber : colors::coral;
        } else if (particle == 0) {
            color = colors::white;
        }
        color.a = particle == 0 ? 245 : 220;
        emit_particle(
            origin + jitter,
            backward.rotated(spread) * speed,
            0.28F + random_unit() * 0.24F,
            3.0F + random_unit() * 4.5F,
            color);
    }
}

void PlayingState::fire_laser() {
    laser_beam_ = {};
    laser_beam_.width = player_.laser_width();
    laser_beam_.duration = player_.reload_time();
    laser_beam_.life = laser_beam_.duration;

    Vector2 direction = player_.direction().normalized();
    Vector2 origin = player_.position() + direction * 24.0F;
    const std::size_t segment_count = static_cast<std::size_t>(
        1 + player_.laser_reflection_count());
    for (std::size_t index = 0;
         index < segment_count && index < laser_beam_.segments.size();
         ++index) {
        bool hit_x = false;
        bool hit_y = false;
        const float distance =
            ray_distance_to_edge(origin, direction, hit_x, hit_y);
        const Vector2 end = origin + direction * distance;
        laser_beam_.segments[laser_beam_.count++] = {origin, end};
        origin = end;
        if (hit_x) {
            direction.x = -direction.x;
        }
        if (hit_y) {
            direction.y = -direction.y;
        }
    }

    std::array<bool, AsteroidField::capacity> hit_indices{};
    const auto& asteroids = asteroid_field_.asteroids();
    for (std::size_t asteroid_index = 0;
         asteroid_index < asteroids.size();
         ++asteroid_index) {
        const Asteroid& asteroid = asteroids[asteroid_index];
        if (!asteroid.alive) {
            continue;
        }
        for (std::size_t segment_index = 0;
             segment_index < laser_beam_.count;
             ++segment_index) {
            const LaserSegment& segment = laser_beam_.segments[segment_index];
            if (segment_hits_circle(
                    segment.start,
                    segment.end,
                    asteroid.position,
                    asteroid.radius + laser_beam_.width / 2.0F)) {
                hit_indices[asteroid_index] = true;
                break;
            }
        }
    }

    for (std::size_t index = 0; index < hit_indices.size(); ++index) {
        if (!hit_indices[index]) {
            continue;
        }
        const Asteroid hit = asteroid_field_.asteroids()[index];
        const bool destroyed = asteroid_field_.apply_damage(
            index, player_.laser_damage());
        if (destroyed) {
            award_asteroid_xp(hit.position, hit.size, hit.kind);
            spawn_impact(hit.position, hit.radius);
            asteroid_field_.destroy_and_split(index);
            context_.audio.play_sound(context_.resources.impact_sound);
        } else {
            spawn_hit(hit.position, hit.radius);
        }
    }
}

void PlayingState::detonate_cannon(
    Vector2 position,
    float projectile_radius,
    int damage,
    int ignored_asteroid) {
    spawn_cannon_blast(position, projectile_radius);
    const float blast_radius = 110.0F + projectile_radius * 4.0F;
    std::array<std::size_t, AsteroidField::capacity> targets{};
    std::size_t target_count = 0;
    const auto& asteroids = asteroid_field_.asteroids();
    for (std::size_t index = 0; index < asteroids.size(); ++index) {
        const Asteroid& asteroid = asteroids[index];
        if (!asteroid.alive || static_cast<int>(index) == ignored_asteroid) {
            continue;
        }
        const Vector2 difference = asteroid.position - position;
        const float reach = blast_radius + asteroid.radius;
        if (difference.length_squared() <= reach * reach) {
            targets[target_count++] = index;
        }
    }

    const int splash_damage = std::max(1, damage / 2);
    for (std::size_t target = 0; target < target_count; ++target) {
        const std::size_t index = targets[target];
        const Asteroid hit = asteroid_field_.asteroids()[index];
        const bool destroyed = asteroid_field_.apply_damage(index, splash_damage);
        if (destroyed) {
            award_asteroid_xp(hit.position, hit.size, hit.kind);
            spawn_impact(hit.position, hit.radius);
            asteroid_field_.destroy_and_split(index);
            context_.audio.play_sound(context_.resources.impact_sound);
        } else {
            spawn_hit(hit.position, hit.radius);
        }
    }
}

void PlayingState::spawn_muzzle_flash() {
    const Vector2 forward = player_.direction();
    const Vector2 origin = player_.position() + forward * 28.0F;
    for (int particle = 0; particle < 9; ++particle) {
        const float spread = (random_unit() - 0.5F) * 54.0F;
        const float speed = 90.0F + random_unit() * 120.0F;
        SDL_Color color = particle % 3 == 0
            ? colors::white
            : weapon_color(player_.ship_type());
        if (player_.ship_type() == ShipType::cannon && particle % 3 == 2) {
            color = colors::coral;
        }
        emit_particle(
            origin,
            forward.rotated(spread) * speed,
            0.07F + random_unit() * 0.1F,
            3.0F + random_unit() * 5.0F,
            color);
    }
    SDL_Color flash_color = weapon_color(player_.ship_type());
    flash_color.a = 220;
    emit_shockwave(origin, 0.12F, 15.0F, flash_color);
}

void PlayingState::spawn_hit(Vector2 position, float radius) {
    for (int particle = 0; particle < 7; ++particle) {
        const float angle = random_unit() * 360.0F;
        const float speed = 35.0F + random_unit() * 65.0F;
        SDL_Color color = particle % 2 == 0 ? colors::white : colors::amber;
        color.a = 225;
        emit_particle(
            position,
            Vector2{0.0F, -1.0F}.rotated(angle) * speed,
            0.12F + random_unit() * 0.14F,
            2.0F + random_unit() * 3.0F,
            color);
    }
    emit_shockwave(position, 0.16F, std::max(10.0F, radius * 0.65F), colors::white);
    screen_shake_ = std::max(screen_shake_, 1.2F);
}

void PlayingState::spawn_cannon_blast(Vector2 position, float radius) {
    const int count = 26 + static_cast<int>(radius);
    for (int particle = 0; particle < count; ++particle) {
        const float angle =
            360.0F * static_cast<float>(particle) / static_cast<float>(count) +
            (random_unit() - 0.5F) * 18.0F;
        const float speed = 80.0F + random_unit() * 190.0F;
        const SDL_Color color = particle % 4 == 0
            ? colors::white
            : particle % 4 == 1 ? colors::amber
            : particle % 4 == 2 ? colors::coral
                                : SDL_Color{255, 118, 35, 235};
        emit_particle(
            position,
            Vector2{0.0F, -1.0F}.rotated(angle) * speed,
            0.22F + random_unit() * 0.38F,
            3.0F + random_unit() * 7.0F,
            color);
    }
    emit_shockwave(position, 0.18F, 34.0F + radius, colors::white);
    emit_shockwave(position, 0.38F, 75.0F + radius * 2.0F, colors::amber);
    emit_shockwave(position, 0.64F, 110.0F + radius * 4.0F, colors::coral);
    screen_shake_ = std::max(screen_shake_, 4.5F + radius * 0.18F);
}

void PlayingState::spawn_impact(Vector2 position, float radius) {
    int remaining = 8;
    for (Debris& shard : debris_) {
        if (shard.life > 0.0F) {
            continue;
        }
        shard = {position, Vector2{0, -70.0F - random_unit() * 100.0F}.rotated(random_unit() * 360.0F),
            0.8F + random_unit() * 0.6F, random_unit() * 360.0F,
            (random_unit() - 0.5F) * 480.0F,
            radius * (0.12F + random_unit() * 0.13F), particle_seed_};
        if (--remaining == 0) {
            break;
        }
    }
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

void PlayingState::spawn_upgrade_pickup() {
    Vector2 position;
    for (int attempt = 0; attempt < 10; ++attempt) {
        position = {
            -320.0F + random_unit() * 640.0F,
            -190.0F + random_unit() * 410.0F,
        };
        if ((position - player_.position()).length_squared() > 130.0F * 130.0F) {
            break;
        }
    }
    upgrade_pickup_ = {position, true};
    emit_shockwave(position, 0.7F, 34.0F, colors::violet);
}

void PlayingState::update_upgrade_pickup() {
    if (!upgrade_pickup_.active) {
        return;
    }
    const Vector2 difference = upgrade_pickup_.position - player_.position();
    const float pickup_radius = player_.collision_radius() + 17.0F;
    if (difference.length_squared() > pickup_radius * pickup_radius) {
        return;
    }
    upgrade_pickup_.active = false;
    if (player_.upgrade_weapon()) {
        upgrade_banner_timer_ = 1.5F;
        screen_shake_ = std::max(screen_shake_, 2.5F);
        for (int particle = 0; particle < 24; ++particle) {
            const float angle = static_cast<float>(particle) * 15.0F;
            const float speed = 55.0F + random_unit() * 55.0F;
            SDL_Color color = particle % 2 == 0 ? colors::cyan : colors::violet;
            emit_particle(
                player_.position(),
                Vector2{0.0F, -1.0F}.rotated(angle) * speed,
                0.35F + random_unit() * 0.3F,
                3.0F + random_unit() * 3.0F,
                color);
        }
        emit_shockwave(player_.position(), 0.55F, 65.0F, colors::amber);
    }
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
