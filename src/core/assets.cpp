#include "game/core/assets.hpp"

#include <SDL2/SDL.h>

#include <system_error>
#include <vector>

namespace game {

Assets::Assets(const std::filesystem::path& executable) {
    std::error_code error;
    const auto executable_path = std::filesystem::absolute(executable, error);
    const auto executable_directory = executable_path.parent_path();
    const auto working_directory = std::filesystem::current_path(error);

    std::vector<std::filesystem::path> candidates;
    if (char* base_path = SDL_GetBasePath(); base_path != nullptr) {
        candidates.emplace_back(std::filesystem::path(base_path) / "assets");
        SDL_free(base_path);
    }
    candidates.insert(candidates.end(), {
        executable_directory / "assets",
        executable_directory.parent_path() / "assets",
        executable_directory.parent_path() / "Resources" / "assets",
        working_directory / "assets",
    });

    for (const auto& candidate : candidates) {
        if (std::filesystem::is_directory(candidate / "images", error)) {
            root_ = std::filesystem::weakly_canonical(candidate, error);
            return;
        }
    }
    root_ = working_directory / "assets";
}

std::filesystem::path Assets::image(std::string_view filename) const {
    return root_ / "images" / filename;
}

std::filesystem::path Assets::audio(std::string_view filename) const {
    return root_ / "audio" / filename;
}

}  // namespace game
