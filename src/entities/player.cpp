#include "game/entities/player.hpp"

#include "game/core/config.hpp"

#include <cmath>

namespace game {

namespace {

constexpr float acceleration = 216.0F;
constexpr float maximum_speed = 120.0F;
constexpr float bullet_speed = 240.0F;
constexpr float muzzle_distance = 20.0F;
constexpr float rotation_speed = 240.0F;

}  // namespace

Player::Player() {
    reset();
}

void Player::reset() {
    lives_ = starting_lives;
    reset_position();
}

void Player::reset_position() {
    previous_angle_ = 0.0F;
    angle_ = 0.0F;
    previous_position_ = {};
    position_ = {};
    velocity_ = {};
    for (Bullet& bullet : bullets_) {
        bullet = {};
    }
}

void Player::accelerate(float delta_time) {
    velocity_ += direction() * (acceleration * delta_time);
    velocity_ = velocity_.limited(maximum_speed);
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
    for (Bullet& bullet : bullets_) {
        if (bullet.alive) {
            continue;
        }
        const Vector2 heading = direction();
        bullet.position = position_ + heading * muzzle_distance;
        bullet.previous_position = bullet.position;
        bullet.velocity = heading * bullet_speed;
        bullet.alive = true;
        return true;
    }
    return false;
}

void Player::update(float delta_time, PlayerInput input) {
    previous_position_ = position_;
    previous_angle_ = angle_;
    rotate(input.turn * rotation_speed * delta_time);
    if (input.thrust) {
        accelerate(delta_time);
    }

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
    }
}

void Player::cull_bullets() {
    for (Bullet& bullet : bullets_) {
        if (!bullet.alive) {
            continue;
        }
        if (bullet.position.x < -config::screen_width / 2.0F ||
            bullet.position.x > config::screen_width / 2.0F ||
            bullet.position.y < -config::screen_height / 2.0F ||
            bullet.position.y > config::screen_height / 2.0F) {
            bullet.alive = false;
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

}  // namespace game
