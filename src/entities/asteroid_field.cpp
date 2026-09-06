#include "game/entities/asteroid_field.hpp"

#include "game/core/config.hpp"

#include <algorithm>
#include <cmath>

namespace game {

float Asteroid::render_angle(float interpolation) const {
    const float difference = std::remainder(angle - previous_angle, 360.0F);
    return previous_angle + difference * interpolation;
}

AsteroidField::AsteroidField() : AsteroidField(std::random_device{}()) {}

AsteroidField::AsteroidField(std::uint32_t seed) : random_(seed) {
    reset();
}

void AsteroidField::reset(
    std::size_t initial_count, float speed_scale, int health_bonus) {
    const Asteroid meteor = asteroids_.back();
    asteroids_ = {};
    active_count_ = std::min(initial_count, capacity - 1);
    health_bonus_ = std::max(0, health_bonus);

    std::uniform_real_distribution<float> x_distribution(
        -config::screen_width / 2.0F, config::screen_width / 2.0F);
    std::uniform_real_distribution<float> y_distribution(
        -config::screen_height / 2.0F, config::screen_height / 2.0F);
    std::uniform_real_distribution<float> direction_distribution(0.0F, 360.0F);
    std::uniform_real_distribution<float> speed_distribution(28.0F, 52.0F);
    std::uniform_real_distribution<float> spin_distribution(20.0F, 80.0F);

    for (std::size_t i = 0; i < active_count_; ++i) {
        Asteroid& asteroid = asteroids_[i];
        configure(asteroid, AsteroidSize::large);
        asteroid.kind = static_cast<AsteroidKind>(i % 3);
        if (asteroid.kind == AsteroidKind::armored) {
            asteroid.maximum_health *= 2;
            asteroid.health = asteroid.maximum_health;
        }
        do {
            asteroid.position = {x_distribution(random_), y_distribution(random_)};
        } while (asteroid.position.length_squared() < 120.0F * 120.0F);

        asteroid.previous_position = asteroid.position;
        const float kind_speed = asteroid.kind == AsteroidKind::armored ? 0.75F
            : asteroid.kind == AsteroidKind::drifter ? 1.4F : 1.0F;
        const float speed = speed_distribution(random_) * std::min(speed_scale, 3.0F) * kind_speed;
        asteroid.velocity =
            Vector2{0.0F, -speed}.rotated(direction_distribution(random_));
        asteroid.angular_velocity = spin_distribution(random_);
        if (direction_distribution(random_) < 180.0F) {
            asteroid.angular_velocity = -asteroid.angular_velocity;
        }
        asteroid.previous_angle = asteroid.angle;
        asteroid.drift_phase = direction_distribution(random_) * 0.0174532925F;
        asteroid.alive = true;
    }
    asteroids_.back() = meteor;
    if (meteor.alive) {
        ++active_count_;
    }
}

void AsteroidField::update(float delta_time) {
    meteor_timer_ -= delta_time;
    if (meteor_timer_ <= 0.0F && !asteroids_.back().alive) {
        auto& meteor = asteroids_.back();
        configure(meteor, AsteroidSize::large);
        meteor.kind = AsteroidKind::meteor;
        std::uniform_real_distribution<float> unit(0.0F, 1.0F);
        const float direction = unit(random_) < 0.5F ? 1.0F : -1.0F;
        meteor.position = {-direction * 440.0F, -170.0F + unit(random_) * 340.0F};
        meteor.previous_position = meteor.position;
        meteor.velocity = {direction * (240.0F + unit(random_) * 60.0F),
            (unit(random_) - 0.5F) * 65.0F};
        meteor.maximum_health = 18 + health_bonus_ * 2;
        meteor.health = meteor.maximum_health;
        meteor.angular_velocity = 100.0F;
        meteor.alive = true;
        ++active_count_;
        meteor_timer_ = 18.0F + unit(random_) * 12.0F;
    }
    for (Asteroid& asteroid : asteroids_) {
        if (!asteroid.alive) {
            continue;
        }
        asteroid.previous_position = asteroid.position;
        asteroid.previous_angle = asteroid.angle;
        if (asteroid.kind == AsteroidKind::drifter) {
            asteroid.drift_phase += delta_time * 1.6F;
            asteroid.velocity = asteroid.velocity.rotated(
                std::sin(asteroid.drift_phase) * 48.0F * delta_time);
        }
        asteroid.position += asteroid.velocity * delta_time;
        asteroid.angle += asteroid.angular_velocity * delta_time;
        asteroid.hit_flash = std::max(0.0F, asteroid.hit_flash - delta_time);
        if (asteroid.kind == AsteroidKind::meteor) {
            if ((asteroid.velocity.x > 0.0F && asteroid.position.x > 450.0F) ||
                (asteroid.velocity.x < 0.0F && asteroid.position.x < -450.0F) ||
                std::abs(asteroid.position.y) > 350.0F) {
                asteroid.alive = false;
                --active_count_;
            }
        } else {
            wrap(asteroid);
        }
    }
}

bool AsteroidField::apply_damage(std::size_t index, int damage) {
    if (index >= asteroids_.size() || !asteroids_[index].alive || damage <= 0) {
        return false;
    }
    Asteroid& asteroid = asteroids_[index];
    asteroid.health = std::max(0, asteroid.health - damage);
    asteroid.hit_flash = 0.1F;
    return asteroid.health == 0;
}

int AsteroidField::collision_index_on_segment(
    Vector2 start, Vector2 end, float radius) const {
    const Vector2 segment = end - start;
    const float segment_length_squared = segment.length_squared();

    for (std::size_t i = 0; i < asteroids_.size(); ++i) {
        const Asteroid& asteroid = asteroids_[i];
        if (!asteroid.alive) {
            continue;
        }

        float projection = 0.0F;
        if (segment_length_squared > 0.0F) {
            projection = std::clamp(
                dot(asteroid.position - start, segment) / segment_length_squared,
                0.0F,
                1.0F);
        }
        const Vector2 closest = start + segment * projection;
        const Vector2 difference = asteroid.position - closest;
        const float collision_distance = asteroid.radius + radius;
        if (difference.length_squared() < collision_distance * collision_distance) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int AsteroidField::collision_index(Vector2 position, float radius) const {
    for (std::size_t i = 0; i < asteroids_.size(); ++i) {
        const Asteroid& asteroid = asteroids_[i];
        if (!asteroid.alive) {
            continue;
        }
        const Vector2 difference{
            asteroid.position.x - position.x,
            asteroid.position.y - position.y,
        };
        const float collision_distance = asteroid.radius + radius;
        if (difference.length_squared() < collision_distance * collision_distance) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void AsteroidField::destroy_and_split(std::size_t index) {
    if (index >= asteroids_.size() || !asteroids_[index].alive) {
        return;
    }

    const Asteroid parent = asteroids_[index];
    asteroids_[index].alive = false;
    --active_count_;
    if (parent.size == AsteroidSize::small || parent.kind == AsteroidKind::meteor) {
        return;
    }

    const AsteroidSize next_size = parent.size == AsteroidSize::large
        ? AsteroidSize::medium
        : AsteroidSize::small;
    const float parent_speed = std::sqrt(parent.velocity.length_squared());
    const float child_speed = std::clamp(parent_speed * 1.2F, 40.0F, 200.0F);
    const Vector2 base_direction = parent.velocity.length_squared() > 0.0F
        ? parent.velocity.normalized()
        : Vector2{0.0F, -1.0F};
    constexpr std::array<float, 3> spread{-35.0F, 0.0F, 35.0F};

    std::size_t spawned = 0;
    for (Asteroid& candidate : asteroids_) {
        if (candidate.alive || &candidate == &asteroids_.back()) {
            continue;
        }
        configure(candidate, next_size);
        candidate.kind = parent.kind;
        candidate.drift_phase = parent.drift_phase + static_cast<float>(spawned) * 2.1F;
        if (candidate.kind == AsteroidKind::armored) {
            candidate.maximum_health *= 2;
            candidate.health = candidate.maximum_health;
        }
        candidate.position = parent.position;
        candidate.previous_position = candidate.position;
        candidate.velocity = base_direction.rotated(spread[spawned]) * child_speed;
        candidate.angular_velocity =
            parent.angular_velocity * (spawned % 2 == 0 ? 1.2F : -1.2F);
        candidate.alive = true;
        ++active_count_;
        ++spawned;
        if (spawned == spread.size()) {
            break;
        }
    }
}

void AsteroidField::configure(Asteroid& asteroid, AsteroidSize size) {
    asteroid = {};
    asteroid.shape_seed = random_();
    asteroid.size = size;
    if (size == AsteroidSize::large) {
        asteroid.radius = 35.0F;
        asteroid.maximum_health = 3 + health_bonus_;
    } else if (size == AsteroidSize::medium) {
        asteroid.radius = 20.0F;
        asteroid.maximum_health = 2 + health_bonus_ / 2;
    } else {
        asteroid.radius = 10.0F;
        asteroid.maximum_health = 1 + health_bonus_ / 3;
    }
    asteroid.health = asteroid.maximum_health;
}

void AsteroidField::wrap(Asteroid& asteroid) const {
    if (asteroid.position.x < -config::screen_width / 2.0F) {
        asteroid.position.x = config::screen_width / 2.0F;
        asteroid.previous_position.x = asteroid.position.x;
    } else if (asteroid.position.x > config::screen_width / 2.0F) {
        asteroid.position.x = -config::screen_width / 2.0F;
        asteroid.previous_position.x = asteroid.position.x;
    }
    if (asteroid.position.y < -config::screen_height / 2.0F) {
        asteroid.position.y = config::screen_height / 2.0F;
        asteroid.previous_position.y = asteroid.position.y;
    } else if (asteroid.position.y > config::screen_height / 2.0F) {
        asteroid.position.y = -config::screen_height / 2.0F;
        asteroid.previous_position.y = asteroid.position.y;
    }
}

}  // namespace game
