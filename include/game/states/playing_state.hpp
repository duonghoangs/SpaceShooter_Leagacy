#pragma once

#include "game/core/context.hpp"
#include "game/core/state.hpp"
#include "game/entities/asteroid_field.hpp"
#include "game/entities/player.hpp"
#include "game/ui/ui_renderer.hpp"

#include <array>
#include <cstdint>

namespace game {

class PlayingState final : public State {
public:
    explicit PlayingState(Context context);

    StateId handle_event(const SDL_Event& event) override;
    StateId update(float delta_time) override;
    void render(float interpolation) override;

private:
    struct Particle {
        Vector2 position;
        Vector2 velocity;
        SDL_Color color{255, 255, 255, 255};
        float life = 0.0F;
        float duration = 0.0F;
        float size = 0.0F;
    };

    struct Shockwave {
        Vector2 position;
        SDL_Color color{255, 255, 255, 255};
        float life = 0.0F;
        float duration = 0.0F;
        float radius = 0.0F;
    };

    [[nodiscard]] StateId pause_action(std::size_t option);
    void update_particles(float delta_time);
    void spawn_exhaust();
    void spawn_muzzle_flash();
    void spawn_impact(Vector2 position, float radius);
    void emit_particle(
        Vector2 position,
        Vector2 velocity,
        float duration,
        float size,
        SDL_Color color);
    void emit_shockwave(
        Vector2 position,
        float duration,
        float radius,
        SDL_Color color);
    [[nodiscard]] float random_unit();

    Context context_;
    UiRenderer ui_;
    Player player_;
    AsteroidField asteroid_field_;
    bool thrusting_ = false;
    bool turning_left_ = false;
    bool turning_right_ = false;
    bool firing_ = false;
    float fire_cooldown_ = 0.0F;
    float invulnerability_ = 0.0F;
    std::size_t wave_ = 1;
    int score_ = 0;
    float hint_timer_ = 5.0F;
    float elapsed_ = 0.0F;
    float pause_amount_ = 0.0F;
    float wave_banner_timer_ = 1.6F;
    float exhaust_timer_ = 0.0F;
    float screen_shake_ = 0.0F;
    bool paused_ = false;
    std::size_t pause_selected_ = 0;
    std::array<Particle, 160> particles_{};
    std::array<Shockwave, 8> shockwaves_{};
    std::uint32_t particle_seed_ = 0xA57E21U;
};

}  // namespace game
