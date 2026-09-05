cmake_minimum_required(VERSION 3.20)

project(Asteroids VERSION 1.0.0 LANGUAGES CXX)

option(ASTEROIDS_BUILD_TESTS "Build game logic tests" ON)
option(ASTEROIDS_ENABLE_AUDIO "Enable SDL2_mixer audio" ON)

add_library(game_logic
    src/entities/asteroid_field.cpp
    src/entities/player.cpp
)
target_include_directories(game_logic PUBLIC include)
target_compile_features(game_logic PUBLIC cxx_std_20)

add_executable(asteroids
    src/main.cpp
    src/core/application.cpp
    src/core/assets.cpp
    src/core/audio.cpp
    src/core/graphics.cpp
    src/states/game_over_state.cpp
    src/states/menu_state.cpp
    src/states/playing_state.cpp
    src/ui/ui_renderer.cpp
)
target_include_directories(asteroids PRIVATE include)
target_compile_features(asteroids PRIVATE cxx_std_20)
target_link_libraries(asteroids PRIVATE game_logic)

if(MSVC)
    target_compile_options(game_logic PRIVATE /W4 /permissive-)
    target_compile_options(asteroids PRIVATE /W4 /permissive-)
else()
    target_compile_options(game_logic PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(asteroids PRIVATE -Wall -Wextra -Wpedantic)
endif()

set(GAME_HAS_AUDIO 0)
if(APPLE)
    find_path(SDL2_INCLUDE_ROOT SDL2/SDL.h PATHS /opt/homebrew/include /usr/local/include)
    find_library(SDL2_LIBRARY SDL2 PATHS /opt/homebrew/lib /usr/local/lib NO_DEFAULT_PATH)
    find_library(SDL2_IMAGE_LIBRARY SDL2_image PATHS /opt/homebrew/lib /usr/local/lib NO_DEFAULT_PATH)

    if(SDL2_INCLUDE_ROOT AND SDL2_LIBRARY AND SDL2_IMAGE_LIBRARY)
        target_include_directories(asteroids PRIVATE ${SDL2_INCLUDE_ROOT})
        target_link_libraries(asteroids PRIVATE ${SDL2_LIBRARY} ${SDL2_IMAGE_LIBRARY})
        set_target_properties(
            asteroids PROPERTIES BUILD_RPATH "/opt/homebrew/lib;/usr/local/lib"
        )
        if(ASTEROIDS_ENABLE_AUDIO)
            find_library(
                SDL2_MIXER_LIBRARY SDL2_mixer
                PATHS /opt/homebrew/lib /usr/local/lib NO_DEFAULT_PATH
            )
        endif()
    else()
        find_library(SDL2_LIBRARY SDL2 REQUIRED PATHS /Library/Frameworks)
        find_library(SDL2_IMAGE_LIBRARY SDL2_image REQUIRED PATHS /Library/Frameworks)
        target_link_libraries(asteroids PRIVATE ${SDL2_LIBRARY} ${SDL2_IMAGE_LIBRARY})
        set_target_properties(asteroids PROPERTIES BUILD_RPATH /Library/Frameworks)
        if(ASTEROIDS_ENABLE_AUDIO)
            find_library(SDL2_MIXER_LIBRARY SDL2_mixer PATHS /Library/Frameworks)
        endif()
    endif()

    if(ASTEROIDS_ENABLE_AUDIO AND SDL2_MIXER_LIBRARY)
        target_link_libraries(asteroids PRIVATE ${SDL2_MIXER_LIBRARY})
        set(GAME_HAS_AUDIO 1)
    endif()
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    pkg_check_modules(SDL2_IMAGE REQUIRED IMPORTED_TARGET SDL2_image)
    target_link_libraries(asteroids PRIVATE PkgConfig::SDL2 PkgConfig::SDL2_IMAGE)

    if(ASTEROIDS_ENABLE_AUDIO)
        pkg_check_modules(SDL2_MIXER QUIET IMPORTED_TARGET SDL2_mixer)
        if(SDL2_MIXER_FOUND)
            target_link_libraries(asteroids PRIVATE PkgConfig::SDL2_MIXER)
            set(GAME_HAS_AUDIO 1)
        endif()
    endif()
endif()

target_compile_definitions(asteroids PRIVATE GAME_HAS_AUDIO=${GAME_HAS_AUDIO})

add_custom_command(TARGET asteroids POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:asteroids>/assets
)

if(ASTEROIDS_BUILD_TESTS)
    enable_testing()
    add_executable(game_logic_tests tests/game_logic_tests.cpp)
    target_link_libraries(game_logic_tests PRIVATE game_logic)
    add_test(NAME game_logic_tests COMMAND game_logic_tests)

    add_executable(game_logic_benchmark tests/game_logic_benchmark.cpp)
    target_link_libraries(game_logic_benchmark PRIVATE game_logic)
endif()

install(TARGETS asteroids RUNTIME DESTINATION bin)
install(DIRECTORY assets DESTINATION bin)
