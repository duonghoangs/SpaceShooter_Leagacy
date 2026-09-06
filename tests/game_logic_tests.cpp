#include "game/entities/asteroid_field.hpp"
#include "game/entities/player.hpp"
#include "game/math/vector2.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <iostream>

namespace {

bool nearly_equal(float left, float right, float tolerance = 0.001F) {
    return std::fabs(left - right) <= tolerance;
}

std::size_t live_bullets(const game::Player& player) {
    std::size_t count = 0;
    for (const game::Bullet& bullet : player.bullets()) {
        count += static_cast<std::size_t>(bullet.alive);
    }
    return count;
}

void test_vector_math() {
    const game::Vector2 vector{3.0F, 4.0F};
    const game::Vector2 normalized = vector.normalized();
    assert(nearly_equal(normalized.length_squared(), 1.0F));

    const game::Vector2 rotated = game::Vector2{0.0F, -1.0F}.rotated(90.0F);
    assert(nearly_equal(rotated.x, 1.0F));
    assert(nearly_equal(rotated.y, 0.0F));
}

void test_player_lifecycle() {
    game::Player player(game::ShipType::twin);
    assert(player.lives() == game::Player::starting_lives);
    assert(player.shoot());
    assert(player.bullets()[0].alive);

    player.update(1.0F / 60.0F, {.turn = 1.0F, .thrust = true});
    assert(player.bullets()[0].position.y < 0.0F);
    assert(player.render_angle(0.5F) > 0.0F);
    assert(player.render_position(1.0F).length_squared() > 0.0F);

    assert(!player.take_hit());
    assert(!player.take_hit());
    assert(player.take_hit());
    assert(player.lives() == 0);
}

void test_inertial_flight() {
    constexpr float timestep = 1.0F / 60.0F;
    game::Player laser(game::ShipType::laser);
    game::Player cannon(game::ShipType::cannon);
    for (int frame = 0; frame < 60; ++frame) {
        laser.update(timestep, {.thrust = true});
        cannon.update(timestep, {.thrust = true});
    }
    const float laser_speed = std::sqrt(laser.velocity().length_squared());
    const float cannon_speed = std::sqrt(cannon.velocity().length_squared());
    assert(laser.throttle() > 0.9F);
    assert(cannon.throttle() > 0.9F);
    assert(laser_speed > cannon_speed);

    const game::Vector2 drifting_from = laser.position();
    for (int frame = 0; frame < 30; ++frame) {
        laser.update(timestep);
    }
    assert((laser.position() - drifting_from).length_squared() > 0.0F);
    assert(laser.throttle() < 0.1F);

    const float speed_before_brake =
        std::sqrt(laser.velocity().length_squared());
    for (int frame = 0; frame < 60; ++frame) {
        laser.update(timestep, {.brake = true});
    }
    assert(std::sqrt(laser.velocity().length_squared()) < speed_before_brake);

    game::Player turning(game::ShipType::twin);
    for (int frame = 0; frame < 15; ++frame) {
        turning.update(timestep, {.turn = 1.0F});
    }
    const float angular_speed = turning.angular_velocity();
    assert(angular_speed > 0.0F);
    turning.update(timestep);
    assert(turning.angular_velocity() > 0.0F);
    assert(turning.angular_velocity() < angular_speed);
}

void test_asteroid_collision_and_split() {
    game::AsteroidField field(7);
    assert(field.active_count() == 4);

    const game::Vector2 first_position = field.asteroids()[0].position;
    const int index = field.collision_index(first_position, 1.0F);
    assert(index == 0);
    assert(field.asteroids()[0].maximum_health == 3);
    assert(!field.apply_damage(0, 1));
    assert(field.asteroids()[0].health == 2);
    assert(field.apply_damage(0, 2));

    field.destroy_and_split(static_cast<std::size_t>(index));
    assert(field.active_count() == 6);

    field.reset(6, 1.5F);
    assert(field.active_count() == 6);

    field.reset(3, 1.0F, 4);
    assert(field.asteroids()[0].maximum_health == 7);
}

void test_ship_loadouts_and_upgrades() {
    game::Player laser(game::ShipType::laser);
    assert(!laser.shoot());
    assert(live_bullets(laser) == 0);
    assert(nearly_equal(laser.reload_time(), 0.16F));
    assert(laser.laser_damage() == 1);
    assert(nearly_equal(laser.laser_width(), 3.0F));
    assert(laser.laser_reflection_count() == 0);

    game::Player twin(game::ShipType::twin);
    for (int level = 1; level < twin.maximum_weapon_level(); ++level) {
        assert(twin.upgrade_weapon());
    }
    assert(twin.shoot());
    assert(live_bullets(twin) == 2);
    assert(!twin.upgrade_weapon());

    game::Player cannon(game::ShipType::cannon);
    assert(cannon.shoot());
    assert(live_bullets(cannon) == 1);
    assert(cannon.bullets()[0].damage == 3);
    assert(cannon.bullets()[0].radius == 6.0F);
    assert(nearly_equal(cannon.reload_time(), 1.32F));
    assert(!nearly_equal(twin.reload_time(), laser.reload_time()));
    assert(!nearly_equal(twin.reload_time(), cannon.reload_time()));

    game::Player damage_laser(game::ShipType::laser);
    for (int level = 1; level < 5; ++level) {
        assert(damage_laser.upgrade_weapon());
    }
    assert(damage_laser.laser_damage() == 5);
    assert(nearly_equal(damage_laser.laser_width(), 3.0F));

    game::Player size_laser(game::ShipType::laser);
    for (int level = 1; level < 10; ++level) {
        assert(size_laser.upgrade_weapon());
    }
    assert(size_laser.laser_damage() == 5);
    assert(nearly_equal(size_laser.laser_width(), 14.0F));
    assert(size_laser.laser_reflection_count() == 0);

    game::Player ricochet_laser(game::ShipType::laser);
    for (int level = 1; level < ricochet_laser.maximum_weapon_level(); ++level) {
        assert(ricochet_laser.upgrade_weapon());
    }
    assert(ricochet_laser.weapon_level() == 15);
    assert(!ricochet_laser.upgrade_weapon());
    assert(!ricochet_laser.shoot());
    assert(ricochet_laser.laser_reflection_count() == 5);
}

void test_projectiles_die_at_screen_edge() {
    game::Player twin(game::ShipType::twin);
    assert(twin.shoot());
    twin.update(1.0F);
    assert(live_bullets(twin) == 0);

    game::Player cannon(game::ShipType::cannon);
    assert(cannon.shoot());
    cannon.update(1.5F);
    assert(!cannon.bullets()[0].alive);
    assert(cannon.bullets()[0].explosion_pending);
}

void test_swept_collision() {
    game::AsteroidField field(11);
    const game::Vector2 target = field.asteroids()[0].position;
    const game::Vector2 start{target.x - 100.0F, target.y};
    const game::Vector2 end{target.x + 100.0F, target.y};

    assert(field.collision_index(start, 1.0F) == -1);
    assert(field.collision_index_on_segment(start, end, 1.0F) == 0);
}

void test_asteroid_variants() {
    game::AsteroidField field(19);
    assert(field.asteroids()[1].kind == game::AsteroidKind::armored);
    assert(field.asteroids()[1].maximum_health == 6);
    assert(!field.apply_damage(1, 3));
    const auto velocity = field.asteroids()[2].velocity;
    const float speed_squared = velocity.length_squared();
    for (int tick = 0; tick < 60; ++tick) {
        field.update(1.0F / 60.0F);
    }
    const auto curved = field.asteroids()[2].velocity;
    assert((curved - velocity).length_squared() > 1.0F);
    assert(std::abs(curved.length_squared() - speed_squared) < speed_squared * 0.001F);
    field.destroy_and_split(1);
    assert(field.active_count() == 6);
    assert(field.asteroids()[1].kind == game::AsteroidKind::armored);
    assert(field.asteroids()[1].maximum_health == 4);
    for (const auto& asteroid : field.asteroids()) {
        if (asteroid.alive) {
            assert(asteroid.velocity.length_squared() <= 200.0F * 200.0F);
        }
    }
}

void test_meteor_lifecycle() {
    game::AsteroidField field(42);
    for (int tick = 0; tick < 610; ++tick) {
        field.update(1.0F / 60.0F);
    }
    const auto index = game::AsteroidField::capacity - 1;
    const auto meteor = field.asteroids()[index];
    assert(meteor.alive && meteor.kind == game::AsteroidKind::meteor);
    assert(meteor.maximum_health >= 18);
    assert(meteor.velocity.length_squared() >= 240.0F * 240.0F);
    assert(field.active_count() == field.wave_count() + 1);
    field.reset(5);
    assert(field.asteroids()[index].position.x == meteor.position.x);
    assert(field.wave_count() == 5);
    assert(!field.apply_damage(index, 1));
    assert(field.apply_damage(index, meteor.maximum_health));
    field.destroy_and_split(index);
    assert(!field.asteroids()[index].alive);
    assert(field.active_count() == 5); // No child asteroids or duplicate destruction.
    field.destroy_and_split(index);
    assert(field.active_count() == 5);

    game::AsteroidField escape(42);
    for (int tick = 0; tick < 900; ++tick) {
        escape.update(1.0F / 60.0F);
    }
    assert(!escape.asteroids()[index].alive);
    assert(escape.active_count() == 4);
    for (int tick = 0; tick < 240; ++tick) {
        escape.update(1.0F / 60.0F);
        assert(!escape.asteroids()[index].alive); // No wrapping or immediate respawn.
    }
    bool returned = false;
    for (int tick = 0; tick < 1500; ++tick) {
        escape.update(1.0F / 60.0F);
        returned = returned || escape.asteroids()[index].alive;
        assert(escape.active_count() <= 5);
    }
    assert(returned);
}

}  // namespace

int main() {
    test_vector_math();
    test_player_lifecycle();
    test_inertial_flight();
    test_asteroid_collision_and_split();
    test_ship_loadouts_and_upgrades();
    test_projectiles_die_at_screen_edge();
    test_swept_collision();
    test_asteroid_variants();
    test_meteor_lifecycle();
    std::cout << "All game logic tests passed\n";
}
