#pragma once

#include <SDL2/SDL.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>

namespace game {

struct WindowDeleter {
    void operator()(SDL_Window* window) const noexcept;
};

struct RendererDeleter {
    void operator()(SDL_Renderer* renderer) const noexcept;
};

struct TextureDeleter {
    void operator()(SDL_Texture* texture) const noexcept;
};

class Graphics {
public:
    using Texture = std::unique_ptr<SDL_Texture, TextureDeleter>;

    Graphics() = default;
    ~Graphics();

    Graphics(const Graphics&) = delete;
    Graphics& operator=(const Graphics&) = delete;

    [[nodiscard]] bool initialise();
    [[nodiscard]] Texture load_texture(const std::filesystem::path& path);

    void clear(const Texture& background = {});
    void draw(const Texture& texture, const SDL_FRect& destination);
    void draw_rotated(
        const Texture& texture, const SDL_FRect& destination, double angle);
    void set_color(const Texture& texture, std::uint8_t value);
    void set_tint(const Texture& texture, SDL_Color color);
    void set_alpha(const Texture& texture, std::uint8_t value);
    void fill_rect(const SDL_FRect& rectangle, SDL_Color color);
    void fill_rects(std::span<const SDL_FRect> rectangles, SDL_Color color);
    void stroke_rect(
        const SDL_FRect& rectangle, SDL_Color color, float thickness = 1.0F);
    [[nodiscard]] bool save_screenshot(const std::filesystem::path& path);
    void present();
    void show_cursor(bool visible);

private:
    std::unique_ptr<SDL_Window, WindowDeleter> window_;
    std::unique_ptr<SDL_Renderer, RendererDeleter> renderer_;
};

}  // namespace game
