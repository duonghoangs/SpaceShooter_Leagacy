#pragma once

#include <filesystem>
#include <string_view>

namespace game {

class Assets {
public:
    explicit Assets(const std::filesystem::path& executable);

    [[nodiscard]] std::filesystem::path image(std::string_view filename) const;
    [[nodiscard]] std::filesystem::path audio(std::string_view filename) const;
    [[nodiscard]] const std::filesystem::path& root() const { return root_; }

private:
    std::filesystem::path root_;
};

}  // namespace game
