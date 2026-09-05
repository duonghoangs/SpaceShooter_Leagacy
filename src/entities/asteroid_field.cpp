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

void AsteroidField::reset(std::size_t initial_count, float speed_scale) {
    asteroids_ = {};
    active_count_ = std::min(initial_count, capacity);

    std::uniform_real_distribution<float> x_distribution(
        -config::screen_width / 2.0F, config::screen_width / 2.0F);
    std::uniform_real_distribution<float> y_distribution(
        -config::screen_height / 2.0F, config::screen_height / 2.0F);
    std::uniform_real_distribution<float> direction_distribution(0.0F, 360.0F);
    std::uniform_real_distribution<float> speed_distribution(10.0F, 35.0F);
    std::uniform_real_distribution<float> spin_distribution(20.0F, 80.0F);

    for (std::size_t i = 0; i < active_count_; ++i) {
        Asteroid& asteroid = asteroids_[i];
        configure(asteroid, AsteroidSize::large);
        do {
            asteroid.position = {x_distribution(random_), y_distribution(random_)};
        } while (asteroid.position.length_squared() < 120.0F * 120.0F);

        asteroid.previous_position = asteroid.position;
        const float speed = speed_distribution(random_) * speed_scale;
        asteroid.velocity =
            Vector2{0.0F, -speed}.rotated(direction_distribution(random_));
        asteroid.angular_velocity = spin_distribution(random_);
        if (direction_distribution(random_) < 180.0F) {
            asteroid.angular_velocity = -asteroid.angular_velocity;
        }
        asteroid.previous_angle = asteroid.angle;
        asteroid.alive = true;
    }
}

void AsteroidField::update(float delta_time) {
    for (Asteroid& asteroid : asteroids_) {
        if (!asteroid.alive) {
            continue;
        }
        asteroid.previous_position = asteroid.position;
        asteroid.previous_angle = asteroid.angle;
        asteroid.position += asteroid.velocity * delta_time;
        asteroid.angle += asteroid.angular_velocity * delta_time;
        wrap(asteroid);
    }
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
    if (parent.size == AsteroidSize::small) {
        return;
    }

    const AsteroidSize next_size = parent.size == AsteroidSize::large
        ? AsteroidSize::medium
        : AsteroidSize::small;
    const float parent_speed = std::sqrt(parent.velocity.length_squared());
    const float child_speed = std::max(parent_speed * 1.2F, 30.0F);
    const Vector2 base_direction = parent.velocity.length_squared() > 0.0F
        ? parent.velocity.normalized()
        : Vector2{0.0F, -1.0F};
    constexpr std::array<float, 3> spread{-35.0F, 0.0F, 35.0F};

    std::size_t spawned = 0;
    for (Asteroid& candidate : asteroids_) {
        if (candidate.alive) {
            continue;
        }
        configure(candidate, next_size);
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
    asteroid.size = size;
    if (size == AsteroidSize::large) {
        asteroid.radius = 35.0F;
    } else if (size == AsteroidSize::medium) {
        asteroid.radius = 20.0F;
    } else {
        asteroid.radius = 10.0F;
    }
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
