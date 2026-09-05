#include "game/entities/asteroid_field.hpp"
#include "game/entities/player.hpp"

#include <chrono>
#include <cstddef>
#include <iostream>

int main() {
    constexpr std::size_t iterations = 1'000'000;
    constexpr float delta_time = 1.0F / 60.0F;

    game::Player player;
    game::AsteroidField field(42);
    std::size_t collisions = 0;

    const auto started = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        player.update(delta_time, {.turn = 0.25F, .thrust = (i % 3) == 0});
        field.update(delta_time);
        if (field.collision_index(player.position(), game::Player::hit_radius) >= 0) {
            ++collisions;
        }
    }
    const auto elapsed = std::chrono::steady_clock::now() - started;
    const double seconds = std::chrono::duration<double>(elapsed).count();

    std::cout << "Logic updates: " << iterations << '\n'
              << "Elapsed: " << seconds << " s\n"
              << "Updates/s: " << static_cast<double>(iterations) / seconds << '\n'
              << "Collision samples: " << collisions << '\n';
}
