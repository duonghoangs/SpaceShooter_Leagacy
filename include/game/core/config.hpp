#pragma once

#include <chrono>

namespace game::config {

inline constexpr int screen_width = 800;
inline constexpr int screen_height = 600;
inline constexpr char window_title[] = "Asteroids";
inline constexpr float fixed_timestep = 1.0F / 60.0F;
inline constexpr float maximum_frame_time = 0.25F;
inline constexpr float maximum_render_rate = 120.0F;
inline constexpr std::chrono::milliseconds game_over_duration{1500};

}  // namespace game::config
