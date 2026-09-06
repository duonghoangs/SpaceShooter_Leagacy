#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace game {

enum class ShipType : std::uint8_t { laser, twin, cannon, count };

inline constexpr std::size_t ship_count =
    static_cast<std::size_t>(ShipType::count);

[[nodiscard]] constexpr std::size_t ship_index(ShipType type) {
    return static_cast<std::size_t>(type);
}

[[nodiscard]] constexpr std::string_view ship_name(ShipType type) {
    switch (type) {
        case ShipType::laser: return "LASER";
        case ShipType::twin: return "TWIN";
        case ShipType::cannon: return "CANNON";
        case ShipType::count: break;
    }
    return "UNKNOWN";
}

}  // namespace game
