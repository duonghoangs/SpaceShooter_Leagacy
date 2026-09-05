#pragma once

#include "game/core/graphics.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace game {

enum class TextAlign { left, center, right };

namespace colors {

inline constexpr SDL_Color white{238, 250, 255, 255};
inline constexpr SDL_Color cyan{104, 232, 255, 255};
inline constexpr SDL_Color amber{255, 184, 77, 255};
inline constexpr SDL_Color coral{255, 92, 112, 255};
inline constexpr SDL_Color violet{174, 125, 255, 255};
inline constexpr SDL_Color magenta = violet;
inline constexpr SDL_Color muted{159, 184, 211, 255};
inline constexpr SDL_Color disabled{91, 111, 139, 255};
inline constexpr SDL_Color border{54, 86, 126, 255};
inline constexpr SDL_Color panel{4, 12, 31, 236};
inline constexpr SDL_Color panel_soft{7, 19, 43, 205};
inline constexpr SDL_Color button{11, 27, 57, 242};
inline constexpr SDL_Color button_hover{24, 57, 91, 250};
inline constexpr SDL_Color overlay{1, 5, 18, 204};
inline constexpr SDL_Color background_veil{1, 8, 25, 48};
inline constexpr SDL_Color ship_shadow{42, 61, 92, 255};
inline constexpr SDL_Color asteroid_shadow{54, 43, 57, 255};

}  // namespace colors

class UiRenderer {
public:
    explicit UiRenderer(Graphics& graphics) : graphics_(graphics) {}

    void text(
        std::string_view value,
        float x,
        float y,
        float scale,
        SDL_Color color = colors::white,
        TextAlign align = TextAlign::left);
    void panel(
        const SDL_FRect& rectangle,
        bool strong = true,
        float opacity = 1.0F);
    void button(
        const SDL_FRect& rectangle,
        std::string_view label,
        bool selected,
        bool enabled = true);
    void button(
        const SDL_FRect& rectangle,
        std::string_view label,
        float focus,
        bool enabled = true,
        float opacity = 1.0F);

    [[nodiscard]] static float text_width(std::string_view value, float scale);

private:
    using Glyph = std::array<std::uint8_t, 7>;
    [[nodiscard]] static Glyph glyph(char character);

    Graphics& graphics_;
    std::array<SDL_FRect, 4096> pixels_{};
};

}  // namespace game
