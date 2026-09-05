#include "game/ui/ui_renderer.hpp"

#include <algorithm>
#include <cctype>
#include <span>

namespace game {

namespace {

SDL_Color mix(SDL_Color from, SDL_Color to, float amount) {
    amount = std::clamp(amount, 0.0F, 1.0F);
    const auto channel = [amount](std::uint8_t start, std::uint8_t end) {
        return static_cast<std::uint8_t>(
            static_cast<float>(start) +
            (static_cast<float>(end) - static_cast<float>(start)) * amount);
    };
    return {
        channel(from.r, to.r),
        channel(from.g, to.g),
        channel(from.b, to.b),
        channel(from.a, to.a),
    };
}

SDL_Color faded(SDL_Color color, float opacity) {
    color.a = static_cast<std::uint8_t>(
        static_cast<float>(color.a) * std::clamp(opacity, 0.0F, 1.0F));
    return color;
}

}  // namespace

float UiRenderer::text_width(std::string_view value, float scale) {
    return value.empty() ? 0.0F : (static_cast<float>(value.size()) * 6.0F - 1.0F) * scale;
}

void UiRenderer::text(
    std::string_view value,
    float x,
    float y,
    float scale,
    SDL_Color color,
    TextAlign align) {
    const float width = text_width(value, scale);
    if (align == TextAlign::center) {
        x -= width / 2.0F;
    } else if (align == TextAlign::right) {
        x -= width;
    }

    std::size_t count = 0;
    for (std::size_t character_index = 0; character_index < value.size(); ++character_index) {
        const Glyph shape = glyph(value[character_index]);
        for (std::size_t row = 0; row < shape.size(); ++row) {
            for (std::size_t column = 0; column < 5; ++column) {
                if ((shape[row] & (1U << (4U - column))) == 0 || count == pixels_.size()) {
                    continue;
                }
                pixels_[count++] = {
                    x + static_cast<float>(character_index * 6 + column) * scale,
                    y + static_cast<float>(row) * scale,
                    scale,
                    scale,
                };
            }
        }
    }
    graphics_.fill_rects(std::span<const SDL_FRect>(pixels_.data(), count), color);
}

void UiRenderer::panel(const SDL_FRect& rectangle, bool strong, float opacity) {
    graphics_.fill_rect(
        rectangle,
        faded(strong ? colors::panel : colors::panel_soft, opacity));
    graphics_.stroke_rect(
        rectangle, faded(colors::cyan, opacity), strong ? 2.0F : 1.0F);
}

void UiRenderer::button(
    const SDL_FRect& rectangle,
    std::string_view label,
    bool selected,
    bool enabled) {
    graphics_.fill_rect(rectangle, selected ? colors::button_hover : colors::button);
    graphics_.stroke_rect(
        rectangle,
        selected ? colors::cyan : colors::border,
        selected ? 2.0F : 1.0F);
    if (selected) {
        graphics_.fill_rect(
            {rectangle.x, rectangle.y, 5.0F, rectangle.h}, colors::magenta);
    }
    text(
        label,
        rectangle.x + rectangle.w / 2.0F,
        rectangle.y + (rectangle.h - 14.0F) / 2.0F,
        2.0F,
        enabled ? colors::white : colors::disabled,
        TextAlign::center);
}

void UiRenderer::button(
    const SDL_FRect& rectangle,
    std::string_view label,
    float focus,
    bool enabled,
    float opacity) {
    focus = std::clamp(focus, 0.0F, 1.0F);
    const float expansion = focus * 3.0F;
    const SDL_FRect animated{
        rectangle.x - expansion,
        rectangle.y - expansion / 2.0F,
        rectangle.w + expansion * 2.0F,
        rectangle.h + expansion,
    };
    graphics_.fill_rect(
        animated, faded(mix(colors::button, colors::button_hover, focus), opacity));
    graphics_.stroke_rect(
        animated,
        faded(mix(colors::border, colors::cyan, focus), opacity),
        1.0F + focus);
    if (focus > 0.0F) {
        const SDL_FRect accent{
            animated.x,
            animated.y,
            5.0F * focus,
            animated.h,
        };
        graphics_.fill_rect(accent, faded(colors::magenta, opacity * focus));
    }
    text(
        label,
        animated.x + animated.w / 2.0F,
        animated.y + (animated.h - 14.0F) / 2.0F,
        2.0F,
        faded(enabled ? colors::white : colors::disabled, opacity),
        TextAlign::center);
}

UiRenderer::Glyph UiRenderer::glyph(char character) {
    switch (static_cast<char>(std::toupper(static_cast<unsigned char>(character)))) {
        case 'A': return {14, 17, 17, 31, 17, 17, 17};
        case 'B': return {30, 17, 17, 30, 17, 17, 30};
        case 'C': return {14, 17, 16, 16, 16, 17, 14};
        case 'D': return {30, 17, 17, 17, 17, 17, 30};
        case 'E': return {31, 16, 16, 30, 16, 16, 31};
        case 'F': return {31, 16, 16, 30, 16, 16, 16};
        case 'G': return {14, 17, 16, 23, 17, 17, 14};
        case 'H': return {17, 17, 17, 31, 17, 17, 17};
        case 'I': return {31, 4, 4, 4, 4, 4, 31};
        case 'J': return {7, 2, 2, 2, 18, 18, 12};
        case 'K': return {17, 18, 20, 24, 20, 18, 17};
        case 'L': return {16, 16, 16, 16, 16, 16, 31};
        case 'M': return {17, 27, 21, 21, 17, 17, 17};
        case 'N': return {17, 25, 21, 19, 17, 17, 17};
        case 'O': return {14, 17, 17, 17, 17, 17, 14};
        case 'P': return {30, 17, 17, 30, 16, 16, 16};
        case 'Q': return {14, 17, 17, 17, 21, 18, 13};
        case 'R': return {30, 17, 17, 30, 20, 18, 17};
        case 'S': return {15, 16, 16, 14, 1, 1, 30};
        case 'T': return {31, 4, 4, 4, 4, 4, 4};
        case 'U': return {17, 17, 17, 17, 17, 17, 14};
        case 'V': return {17, 17, 17, 17, 17, 10, 4};
        case 'W': return {17, 17, 17, 21, 21, 21, 10};
        case 'X': return {17, 17, 10, 4, 10, 17, 17};
        case 'Y': return {17, 17, 10, 4, 4, 4, 4};
        case 'Z': return {31, 1, 2, 4, 8, 16, 31};
        case '0': return {14, 17, 19, 21, 25, 17, 14};
        case '1': return {4, 12, 4, 4, 4, 4, 14};
        case '2': return {14, 17, 1, 2, 4, 8, 31};
        case '3': return {30, 1, 1, 14, 1, 1, 30};
        case '4': return {2, 6, 10, 18, 31, 2, 2};
        case '5': return {31, 16, 16, 30, 1, 1, 30};
        case '6': return {14, 16, 16, 30, 17, 17, 14};
        case '7': return {31, 1, 2, 4, 8, 8, 8};
        case '8': return {14, 17, 17, 14, 17, 17, 14};
        case '9': return {14, 17, 17, 15, 1, 1, 14};
        case ':': return {0, 4, 4, 0, 4, 4, 0};
        case '-': return {0, 0, 0, 31, 0, 0, 0};
        case '/': return {1, 2, 2, 4, 8, 8, 16};
        case '.': return {0, 0, 0, 0, 0, 12, 12};
        default: return {};
    }
}

}  // namespace game
