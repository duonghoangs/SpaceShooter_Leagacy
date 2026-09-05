#pragma once

#include "game/math/vector2.hpp"

#include <array>
#include <cstddef>

namespace game {

struct Bullet {
    Vector2 previous_position;
    Vector2 position;
    Vector2 velocity;
    bool alive = false;

    [[nodiscard]] Vector2 render_position(float interpolation) const {
        return lerp(previous_position, position, interpolation);
    }
};

struct PlayerInput {
    float turn = 0.0F;
    bool thrust = false;
};

class Player {
public:
    static constexpr int starting_lives = 3;
    static constexpr float hit_radius = 15.0F;
    static constexpr std::size_t bullet_capacity = 3;

    Player();

    void reset();
    void reset_position();
    [[nodiscard]] bool shoot();
    void update(float delta_time, PlayerInput input = {});
    void cull_bullets();
    [[nodiscard]] bool take_hit();

    [[nodiscard]] Vector2 direction() const;
    [[nodiscard]] const Vector2& position() const { return position_; }
    [[nodiscard]] float angle() const { return angle_; }
    [[nodiscard]] Vector2 render_position(float interpolation) const;
    [[nodiscard]] float render_angle(float interpolation) const;
    [[nodiscard]] int lives() const { return lives_; }
    [[nodiscard]] const std::array<Bullet, bullet_capacity>& bullets() const {
        return bullets_;
    }
    [[nodiscard]] std::array<Bullet, bullet_capacity>& bullets() { return bullets_; }

private:
    void accelerate(float delta_time);
    void rotate(float degrees);
    [[nodiscard]] bool wrap_position();

    int lives_ = starting_lives;
    float previous_angle_ = 0.0F;
    float angle_ = 0.0F;
    Vector2 previous_position_;
    Vector2 position_;
    Vector2 velocity_;
    std::array<Bullet, bullet_capacity> bullets_;
};

}  // namespace game
