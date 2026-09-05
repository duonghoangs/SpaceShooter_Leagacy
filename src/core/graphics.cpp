#include "game/core/graphics.hpp"

#include "game/core/config.hpp"

#if __has_include(<SDL2/SDL_image.h>)
#include <SDL2/SDL_image.h>
#else
#include <SDL2_image/SDL_image.h>
#endif

#include <array>
#include <iostream>
#include <string>

namespace game {

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
