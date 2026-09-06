#include "game/entities/player.hpp"

#include "game/core/config.hpp"

#include <algorithm>
#include <cmath>

namespace game {

namespace {

constexpr float muzzle_distance = 24.0F;

struct FlightTuning {
    float acceleration;
    float maximum_speed;
    float throttle_response;
    float passive_drag;
    float brake_drag;
    float angular_acceleration;
    float maximum_angular_speed;
    float angular_drag;
};

FlightTuning flight_tuning(ShipType type) {
    if (type == ShipType::laser) {
        return {270.0F, 165.0F, 7.5F, 0.11F, 1.55F, 980.0F, 300.0F, 7.2F};
    }
    if (type == ShipType::twin) {
        return {220.0F, 145.0F, 5.8F, 0.08F, 1.30F, 760.0F, 245.0F, 6.0F};
    }
    return {155.0F, 115.0F, 3.2F, 0.055F, 1.05F, 470.0F, 165.0F, 4.5F};
}

}  // namespace

Player::Player(ShipType ship_type) : ship_type_(ship_type) {
    reset();
}

void Player::reset() {
    lives_ = starting_lives;
    reset_position();
}

void Player::reset_position() {
    previous_angle_ = 0.0F;
    angle_ = 0.0F;
    angular_velocity_ = 0.0F;
    throttle_ = 0.0F;
    previous_position_ = {};
    position_ = {};
    velocity_ = {};
    for (Bullet& bullet : bullets_) {
        bullet = {};
    }
}

void Player::update_flight(float delta_time, PlayerInput input) {
    const FlightTuning tuning = flight_tuning(ship_type_);

    const float throttle_target = input.thrust && !input.brake ? 1.0F : 0.0F;
    const float throttle_rate = input.brake
        ? tuning.throttle_response * 2.2F
        : tuning.throttle_response;
    const float throttle_blend = 1.0F - std::exp(-throttle_rate * delta_time);
    throttle_ += (throttle_target - throttle_) * throttle_blend;

    velocity_ += direction() * (tuning.acceleration * throttle_ * delta_time);
    const float drag = tuning.passive_drag + (input.brake ? tuning.brake_drag : 0.0F);
    velocity_ *= std::exp(-drag * delta_time);

    const float speed_squared = velocity_.length_squared();
    if (speed_squared > tuning.maximum_speed * tuning.maximum_speed) {
        const float speed = std::sqrt(speed_squared);
        const float excess = speed - tuning.maximum_speed;
        const float softened_speed =
            tuning.maximum_speed + excess * std::exp(-5.0F * delta_time);
        velocity_ *= softened_speed / speed;
    }

    angular_velocity_ += input.turn * tuning.angular_acceleration * delta_time;
    angular_velocity_ = std::clamp(
        angular_velocity_, -tuning.maximum_angular_speed,
        tuning.maximum_angular_speed);
    const float angular_drag = std::abs(input.turn) < 0.01F
        ? tuning.angular_drag
        : tuning.angular_drag * 0.18F;
    angular_velocity_ *= std::exp(-angular_drag * delta_time);
    rotate(angular_velocity_ * delta_time);
}

void Player::rotate(float degrees) {
    angle_ += degrees;
    if (angle_ >= 360.0F) {
        angle_ -= 360.0F;
    } else if (angle_ < 0.0F) {
        angle_ += 360.0F;
    }
}

bool Player::shoot() {
    if (ship_type_ == ShipType::laser) {
        return false;
    }

    int projectile_count = 2;
    float projectile_speed = 315.0F + static_cast<float>(weapon_level_ - 1) * 12.0F;
    float range = 430.0F + static_cast<float>(weapon_level_ - 1) * 70.0F;
    float radius = 3.0F;
    int damage = 1 + (weapon_level_ - 1) / 4;

    if (ship_type_ == ShipType::cannon) {
        projectile_count = 1 + (weapon_level_ - 1) / 2;
        projectile_speed = 225.0F + static_cast<float>(weapon_level_ - 1) * 10.0F;
        range = 360.0F + static_cast<float>(weapon_level_ - 1) * 75.0F;
        radius = 6.0F + static_cast<float>(weapon_level_ - 1);
        damage = 3 + (weapon_level_ - 1) * 2;
    }

    std::array<Bullet*, bullet_capacity> available{};
    std::size_t available_count = 0;
    for (Bullet& bullet : bullets_) {
        if (!bullet.alive) {
            available[available_count++] = &bullet;
        }
    }
    if (available_count < static_cast<std::size_t>(projectile_count)) {
        return false;
    }

    const Vector2 forward = direction();
    const Vector2 right = forward.rotated(90.0F);
    for (int index = 0; index < projectile_count; ++index) {
        const float centered_index =
            static_cast<float>(index) - static_cast<float>(projectile_count - 1) / 2.0F;
        float spread = centered_index * 3.0F;
        float lateral = 0.0F;
        if (ship_type_ == ShipType::twin) {
            spread = centered_index * 1.5F;
            lateral = centered_index * 8.0F;
        } else if (ship_type_ == ShipType::cannon) {
            spread = centered_index * 9.0F;
        }

        Bullet& bullet = *available[static_cast<std::size_t>(index)];
        const Vector2 heading = forward.rotated(spread);
        bullet = {};
        bullet.position = position_ + forward * muzzle_distance + right * lateral;
        bullet.previous_position = bullet.position;
        bullet.velocity = heading * projectile_speed;
        bullet.weapon = ship_type_;
        bullet.maximum_range = range;
        bullet.radius = radius;
        bullet.damage = damage;
        bullet.alive = true;
    }
    return true;
}

void Player::update(float delta_time, PlayerInput input) {
    previous_position_ = position_;
    previous_angle_ = angle_;
    update_flight(delta_time, input);

    position_ += velocity_ * delta_time;
    if (wrap_position()) {
        previous_position_ = position_;
    }

    for (Bullet& bullet : bullets_) {
        if (!bullet.alive) {
            continue;
        }
        bullet.previous_position = bullet.position;
        bullet.position += bullet.velocity * delta_time;
        bullet.travelled +=
            std::sqrt(bullet.velocity.length_squared()) * delta_time;
        handle_bullet_bounds(bullet);
    }
}

void Player::cull_bullets() {
    for (Bullet& bullet : bullets_) {
        if (!bullet.alive) {
            continue;
        }
        if (bullet.travelled >= bullet.maximum_range) {
            bullet.alive = false;
            bullet.explosion_pending = bullet.weapon == ShipType::cannon;
        }
    }
}

bool Player::take_hit() {
    if (lives_ > 0) {
        --lives_;
    }
    reset_position();
    return lives_ <= 0;
}

bool Player::upgrade_weapon() {
    if (weapon_level_ >= maximum_weapon_level()) {
        return false;
    }
    ++weapon_level_;
    return true;
}

float Player::reload_time() const {
    if (ship_type_ == ShipType::laser) {
        return 0.16F;
    }
    if (ship_type_ == ShipType::twin) {
        return 0.30F;
    }
    return 1.32F;
}

int Player::laser_damage() const {
    return std::clamp(weapon_level_, 1, 5);
}

float Player::laser_width() const {
    const int size_level = std::clamp(weapon_level_ - 5, 0, 5);
    return 3.0F + static_cast<float>(size_level) * 2.2F;
}

int Player::laser_reflection_count() const {
    return std::clamp(weapon_level_ - 10, 0, 5);
}

int Player::maximum_weapon_level() const {
    return ship_type_ == ShipType::laser
        ? laser_maximum_weapon_level
        : standard_maximum_weapon_level;
}

float Player::collision_radius() const {
    if (ship_type_ == ShipType::laser) {
        return 14.0F;
    }
    if (ship_type_ == ShipType::twin) {
        return 16.0F;
    }
    return 18.0F;
}

Vector2 Player::direction() const {
    return Vector2{0.0F, -1.0F}.rotated(angle_);
}

Vector2 Player::render_position(float interpolation) const {
    return lerp(previous_position_, position_, interpolation);
}

float Player::render_angle(float interpolation) const {
    const float difference = std::remainder(angle_ - previous_angle_, 360.0F);
    return previous_angle_ + difference * interpolation;
}

bool Player::wrap_position() {
    bool wrapped = false;
    if (position_.x < -config::screen_width / 2.0F) {
        position_.x = config::screen_width / 2.0F;
        wrapped = true;
    } else if (position_.x > config::screen_width / 2.0F) {
        position_.x = -config::screen_width / 2.0F;
        wrapped = true;
    }
    if (position_.y < -config::screen_height / 2.0F) {
        position_.y = config::screen_height / 2.0F;
        wrapped = true;
    } else if (position_.y > config::screen_height / 2.0F) {
        position_.y = -config::screen_height / 2.0F;
        wrapped = true;
    }
    return wrapped;
}

void Player::handle_bullet_bounds(Bullet& bullet) const {
    const float half_width = config::screen_width / 2.0F;
    const float half_height = config::screen_height / 2.0F;
    const bool outside_x = bullet.position.x < -half_width ||
        bullet.position.x > half_width;
    const bool outside_y = bullet.position.y < -half_height ||
        bullet.position.y > half_height;
    if (!outside_x && !outside_y) {
        return;
    }

    bullet.alive = false;
    bullet.explosion_pending = bullet.weapon == ShipType::cannon;
}

}  // namespace game
