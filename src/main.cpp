#include "game/core/application.hpp"

#include <filesystem>
#include <string_view>

int main(int argc, char* argv[]) {
    const std::filesystem::path executable = argc > 0 ? argv[0] : "asteroids";
    game::Application application(executable);
    if (argc == 3 && std::string_view(argv[1]) == "--capture-ui") {
        return application.capture_ui(argv[2]);
    }
    return application.run();
}
