#include "game/core/graphics.hpp"

#include "game/core/config.hpp"

#if __has_include(<SDL2/SDL_image.h>)
#include <SDL2/SDL_image.h>
#else
#include <SDL2_image/SDL_image.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <string>

namespace game {

void Graphics::asteroid(const Texture& texture, SDL_FPoint center, float radius,
    float angle, std::uint32_t seed, std::uint8_t alpha, bool meteor) {
    if (!texture) {
        return;
    }
    // Warp the original sprite, preserving its pixels, palette and crater detail.
    std::array<SDL_Vertex, 16> vertices{};
    std::array<int, 54> indices{};
    const float radians = angle * 0.0174532925F;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    // Distinct silhouettes, not merely small variations of the same circle.
    constexpr std::array<float, 5> widths{1.0F, 0.62F, 1.0F, 0.86F, 0.95F};
    constexpr std::array<float, 5> heights{0.68F, 1.0F, 0.95F, 0.88F, 0.8F};
    constexpr std::array<std::array<float, 4>, 5> profiles{{
        {0.65F, 1.0F, 0.95F, 0.7F}, {0.8F, 1.0F, 0.9F, 0.55F},
        {0.95F, 0.60F, 1.0F, 0.7F}, {0.5F, 0.75F, 1.0F, 0.95F},
        {1.0F, 0.85F, 0.55F, 0.9F}}};
    const auto variant = seed % widths.size();
    const float width = meteor ? 1.0F : widths[variant];
    const float height = meteor ? 0.85F : heights[variant];
    for (int row = 0; row < 4; ++row) {
        seed = seed * 1664525U + 1013904223U;
        const float taper = 0.83F + static_cast<float>(seed >> 24) / 255.0F * 0.17F;
        for (int column = 0; column < 4; ++column) {
            const float u = static_cast<float>(column) / 3.0F;
            const float v = static_cast<float>(row) / 3.0F;
            const float profile = meteor ? 1.0F : profiles[variant][row];
            const float shear = !meteor && variant == 3 ? (v - 0.5F) * radius * 0.3F : 0.0F;
            const float x = (u * 2.0F - 1.0F) * radius * width * taper * profile + shear;
            const float y = (v * 2.0F - 1.0F) * radius * height *
                (meteor ? (0.45F + 0.32F * std::sin(u * 3.14159265F)) : 1.0F);
            vertices[row * 4 + column] = {
                {center.x + x * cosine - y * sine, center.y + x * sine + y * cosine},
                {255, 255, 255, alpha}, {u, v}};
        }
    }
    int index = 0;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            const int corner = row * 4 + column;
            for (int vertex : {corner, corner + 1, corner + 4, corner + 1, corner + 5, corner + 4}) {
                indices[index++] = vertex;
            }
        }
    }
    SDL_RenderGeometry(renderer_.get(), texture.get(), vertices.data(),
        static_cast<int>(vertices.size()), indices.data(), static_cast<int>(indices.size()));
}


void WindowDeleter::operator()(SDL_Window* window) const noexcept {
    SDL_DestroyWindow(window);
}

void RendererDeleter::operator()(SDL_Renderer* renderer) const noexcept {
    SDL_DestroyRenderer(renderer);
}

void TextureDeleter::operator()(SDL_Texture* texture) const noexcept {
    SDL_DestroyTexture(texture);
}

Graphics::~Graphics() {
    renderer_.reset();
    window_.reset();
    IMG_Quit();
    SDL_Quit();
}

bool Graphics::initialise() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return false;
    }

    window_.reset(SDL_CreateWindow(
        config::window_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        config::screen_width, config::screen_height, SDL_WINDOW_SHOWN));
    if (!window_) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        return false;
    }

    renderer_.reset(SDL_CreateRenderer(
        window_.get(), -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC));
    if (!renderer_) {
        renderer_.reset(SDL_CreateRenderer(window_.get(), -1, SDL_RENDERER_SOFTWARE));
    }
    if (!renderer_) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << '\n';
        return false;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_RenderSetLogicalSize(
        renderer_.get(), config::screen_width, config::screen_height);
    SDL_SetRenderDrawBlendMode(renderer_.get(), SDL_BLENDMODE_BLEND);

    if ((IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & IMG_INIT_PNG) == 0) {
        std::cerr << "IMG_Init failed: " << IMG_GetError() << '\n';
        return false;
    }
    return true;
}

Graphics::Texture Graphics::load_texture(const std::filesystem::path& path) {
    const std::string key = path.string();
    SDL_Texture* raw_texture = IMG_LoadTexture(renderer_.get(), key.c_str());
    if (raw_texture == nullptr) {
        SDL_Log("Cannot load texture %s: %s", key.c_str(), IMG_GetError());
        return {};
    }
    SDL_SetTextureBlendMode(raw_texture, SDL_BLENDMODE_BLEND);

    return Texture(raw_texture, TextureDeleter{});
}

void Graphics::clear(const Texture& background) {
    SDL_SetRenderDrawColor(renderer_.get(), 0, 0, 0, 255);
    SDL_RenderClear(renderer_.get());
    if (background) {
        SDL_RenderCopy(renderer_.get(), background.get(), nullptr, nullptr);
    }
}

void Graphics::draw(const Texture& texture, const SDL_FRect& destination) {
    if (texture) {
        SDL_RenderCopyF(renderer_.get(), texture.get(), nullptr, &destination);
    }
}

void Graphics::draw_rotated(
    const Texture& texture, const SDL_FRect& destination, double angle) {
    if (texture) {
        SDL_RenderCopyExF(
            renderer_.get(), texture.get(), nullptr, &destination, angle, nullptr,
            SDL_FLIP_NONE);
    }
}

void Graphics::set_color(const Texture& texture, std::uint8_t value) {
    if (texture) {
        SDL_SetTextureColorMod(texture.get(), value, value, value);
    }
}

void Graphics::set_tint(const Texture& texture, SDL_Color color) {
    if (texture) {
        SDL_SetTextureColorMod(texture.get(), color.r, color.g, color.b);
    }
}

void Graphics::set_alpha(const Texture& texture, std::uint8_t value) {
    if (texture) {
        SDL_SetTextureAlphaMod(texture.get(), value);
    }
}

void Graphics::set_blend_mode(const Texture& texture, SDL_BlendMode mode) {
    if (texture) {
        SDL_SetTextureBlendMode(texture.get(), mode);
    }
}

void Graphics::fill_rect(const SDL_FRect& rectangle, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer_.get(), color.r, color.g, color.b, color.a);
    SDL_RenderFillRectF(renderer_.get(), &rectangle);
}

void Graphics::fill_rects(
    std::span<const SDL_FRect> rectangles, SDL_Color color) {
    if (rectangles.empty()) {
        return;
    }
    SDL_SetRenderDrawColor(renderer_.get(), color.r, color.g, color.b, color.a);
    SDL_RenderFillRectsF(
        renderer_.get(), rectangles.data(), static_cast<int>(rectangles.size()));
}

void Graphics::draw_line(
    SDL_FPoint start, SDL_FPoint end, SDL_Color color, float thickness) {
    SDL_SetRenderDrawColor(renderer_.get(), color.r, color.g, color.b, color.a);
    const float delta_x = end.x - start.x;
    const float delta_y = end.y - start.y;
    const float length = std::sqrt(delta_x * delta_x + delta_y * delta_y);
    if (length <= 0.001F) {
        return;
    }

    const float normal_x = -delta_y / length;
    const float normal_y = delta_x / length;
    const int line_count = std::max(1, static_cast<int>(std::ceil(thickness)));
    const float center = static_cast<float>(line_count - 1) / 2.0F;
    for (int line = 0; line < line_count; ++line) {
        const float offset = static_cast<float>(line) - center;
        SDL_RenderDrawLineF(
            renderer_.get(),
            start.x + normal_x * offset,
            start.y + normal_y * offset,
            end.x + normal_x * offset,
            end.y + normal_y * offset);
    }
}

void Graphics::stroke_rect(
    const SDL_FRect& rectangle, SDL_Color color, float thickness) {
    const std::array borders{
        SDL_FRect{rectangle.x, rectangle.y, rectangle.w, thickness},
        SDL_FRect{
            rectangle.x,
            rectangle.y + rectangle.h - thickness,
            rectangle.w,
            thickness,
        },
        SDL_FRect{rectangle.x, rectangle.y, thickness, rectangle.h},
        SDL_FRect{
            rectangle.x + rectangle.w - thickness,
            rectangle.y,
            thickness,
            rectangle.h,
        },
    };
    fill_rects(borders, color);
}

bool Graphics::save_screenshot(const std::filesystem::path& path) {
    std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> surface(
        SDL_CreateRGBSurfaceWithFormat(
            0,
            config::screen_width,
            config::screen_height,
            24,
            SDL_PIXELFORMAT_RGB24),
        SDL_FreeSurface);
    if (!surface) {
        return false;
    }
    if (SDL_RenderReadPixels(
            renderer_.get(),
            nullptr,
            SDL_PIXELFORMAT_RGB24,
            surface->pixels,
            surface->pitch) != 0) {
        return false;
    }
    return SDL_SaveBMP(surface.get(), path.string().c_str()) == 0;
}

void Graphics::present() {
    SDL_RenderPresent(renderer_.get());
}

void Graphics::show_cursor(bool visible) {
    SDL_ShowCursor(visible ? SDL_ENABLE : SDL_DISABLE);
}

}  // namespace game
