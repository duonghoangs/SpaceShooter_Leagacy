#pragma once

#include "game/core/loadout.hpp"
#include "game/math/vector2.hpp"

#include <array>
#include <cstddef>

namespace game {

struct Bullet {
    Vector2 previous_position;
    Vector2 position;
    Vector2 velocity;
    ShipType weapon = ShipType::laser;
    float travelled = 0.0F;
    float maximum_range = 0.0F;
    float radius = 2.0F;
    int damage = 1;
    bool explosion_pending = false;
    bool alive = false;

    [[nodiscard]] Vector2 render_position(float interpolation) const {
        return lerp(previous_position, position, interpolation);
    }
};

struct PlayerInput {
    float turn = 0.0F;
    bool thrust = false;
    bool brake = false;
};

class Player {
public:
    static constexpr int starting_lives = 3;
    static constexpr float hit_radius = 15.0F;
    static constexpr int laser_maximum_weapon_level = 15;
    static constexpr int standard_maximum_weapon_level = 5;
    static constexpr std::size_t bullet_capacity = 32;

    explicit Player(ShipType ship_type = ShipType::laser);

    void reset();
    void reset_position();
    [[nodiscard]] bool shoot();
    void update(float delta_time, PlayerInput input = {});
    void cull_bullets();
    [[nodiscard]] bool take_hit();
    [[nodiscard]] bool upgrade_weapon();

    [[nodiscard]] Vector2 direction() const;
    [[nodiscard]] const Vector2& position() const { return position_; }
    [[nodiscard]] const Vector2& velocity() const { return velocity_; }
    [[nodiscard]] float angle() const { return angle_; }
    [[nodiscard]] float angular_velocity() const { return angular_velocity_; }
    [[nodiscard]] float throttle() const { return throttle_; }
    [[nodiscard]] ShipType ship_type() const { return ship_type_; }
    [[nodiscard]] int weapon_level() const { return weapon_level_; }
    [[nodiscard]] int maximum_weapon_level() const;
    [[nodiscard]] float reload_time() const;
    [[nodiscard]] int laser_damage() const;
    [[nodiscard]] float laser_width() const;
    [[nodiscard]] int laser_reflection_count() const;
    [[nodiscard]] float collision_radius() const;
    [[nodiscard]] Vector2 render_position(float interpolation) const;
    [[nodiscard]] float render_angle(float interpolation) const;
    [[nodiscard]] int lives() const { return lives_; }
    [[nodiscard]] const std::array<Bullet, bullet_capacity>& bullets() const {
        return bullets_;
    }
    [[nodiscard]] std::array<Bullet, bullet_capacity>& bullets() { return bullets_; }

private:
    void update_flight(float delta_time, PlayerInput input);
    void rotate(float degrees);
    [[nodiscard]] bool wrap_position();
    void handle_bullet_bounds(Bullet& bullet) const;

    ShipType ship_type_ = ShipType::laser;
    int weapon_level_ = 1;
    int lives_ = starting_lives;
    float previous_angle_ = 0.0F;
    float angle_ = 0.0F;
    float angular_velocity_ = 0.0F;
    float throttle_ = 0.0F;
    Vector2 previous_position_;
    Vector2 position_;
    Vector2 velocity_;
    std::array<Bullet, bullet_capacity> bullets_;
};

}  // namespace game
