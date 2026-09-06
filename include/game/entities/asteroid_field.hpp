#pragma once

#include "game/math/vector2.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>

namespace game {

enum class AsteroidSize { small, medium, large };
enum class AsteroidKind { rock, armored, drifter, meteor };

struct Asteroid {
    Vector2 previous_position;
    Vector2 position;
    Vector2 velocity;
    AsteroidSize size = AsteroidSize::large;
    AsteroidKind kind = AsteroidKind::rock;
    float drift_phase = 0.0F;
    std::uint32_t shape_seed = 1;
    float radius = 35.0F;
    float previous_angle = 0.0F;
    float angle = 0.0F;
    float angular_velocity = 0.0F;
    int health = 1;
    int maximum_health = 1;
    float hit_flash = 0.0F;
    bool alive = false;

    [[nodiscard]] Vector2 render_position(float interpolation) const {
        return lerp(previous_position, position, interpolation);
    }
    [[nodiscard]] float render_angle(float interpolation) const;
};

class AsteroidField {
public:
    static constexpr std::size_t capacity = 27;

    AsteroidField();
    explicit AsteroidField(std::uint32_t seed);

    void reset(
        std::size_t initial_count = 4,
        float speed_scale = 1.0F,
        int health_bonus = 0);
    void update(float delta_time);
    [[nodiscard]] int collision_index(Vector2 position, float radius) const;
    [[nodiscard]] int collision_index_on_segment(
        Vector2 start, Vector2 end, float radius) const;
    [[nodiscard]] bool apply_damage(std::size_t index, int damage);
    void destroy_and_split(std::size_t index);
    [[nodiscard]] std::size_t active_count() const { return active_count_; }
    [[nodiscard]] std::size_t wave_count() const {
        return active_count_ - (asteroids_.back().alive ? 1U : 0U);
    }
    [[nodiscard]] const std::array<Asteroid, capacity>& asteroids() const {
        return asteroids_;
    }

private:
    void configure(Asteroid& asteroid, AsteroidSize size);
    void wrap(Asteroid& asteroid) const;

    std::mt19937 random_;
    std::array<Asteroid, capacity> asteroids_;
    std::size_t active_count_ = 0;
    int health_bonus_ = 0;
    float meteor_timer_ = 10.0F;
};

}  // namespace game
