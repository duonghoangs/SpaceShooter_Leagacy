#include "game/entities/asteroid_field.hpp"
#include "game/entities/player.hpp"
#include "game/math/vector2.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

bool nearly_equal(float left, float right, float tolerance = 0.001F) {
    return std::fabs(left - right) <= tolerance;
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
    game::Player player;
    assert(player.lives() == game::Player::starting_lives);
    assert(player.shoot());
    assert(player.bullets()[0].alive);

    player.update(1.0F / 60.0F, {.turn = 1.0F, .thrust = true});
    assert(player.bullets()[0].position.y < 0.0F);
    assert(nearly_equal(player.render_angle(0.5F), 2.0F));
    assert(player.render_position(1.0F).length_squared() > 0.0F);

    assert(!player.take_hit());
    assert(!player.take_hit());
    assert(player.take_hit());
    assert(player.lives() == 0);
}

void test_asteroid_collision_and_split() {
    game::AsteroidField field(7);
    assert(field.active_count() == 3);

    const game::Vector2 first_position = field.asteroids()[0].position;
    const int index = field.collision_index(first_position, 1.0F);
    assert(index == 0);

    field.destroy_and_split(static_cast<std::size_t>(index));
    assert(field.active_count() == 5);

    field.reset(6, 1.5F);
    assert(field.active_count() == 6);
}

void test_swept_collision() {
    game::AsteroidField field(11);
    const game::Vector2 target = field.asteroids()[0].position;
    const game::Vector2 start{target.x - 100.0F, target.y};
    const game::Vector2 end{target.x + 100.0F, target.y};

    assert(field.collision_index(start, 1.0F) == -1);
    assert(field.collision_index_on_segment(start, end, 1.0F) == 0);
}

}  // namespace

int main() {
    test_vector_math();
    test_player_lifecycle();
    test_asteroid_collision_and_split();
    test_swept_collision();
    std::cout << "All game logic tests passed\n";
}
