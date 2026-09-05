CXX ?= c++

TARGET := build/asteroids
TEST_TARGET := build/game_logic_tests
BENCHMARK_TARGET := build/game_logic_benchmark
GAME_SOURCES := $(shell find src -name '*.cpp' -print)
GAME_OBJECTS := $(patsubst src/%.cpp,.build/%.o,$(GAME_SOURCES))
LOGIC_SOURCES := src/entities/player.cpp src/entities/asteroid_field.cpp
LOGIC_OBJECTS := $(patsubst src/%.cpp,.build/%.o,$(LOGIC_SOURCES))
TEST_OBJECT := .build/tests/game_logic_tests.o
BENCHMARK_OBJECT := .build/tests/game_logic_benchmark.o
DEPENDENCIES := $(GAME_OBJECTS:.o=.d) $(TEST_OBJECT:.o=.d) $(BENCHMARK_OBJECT:.o=.d)

CPPFLAGS := -Iinclude
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -Wpedantic -MMD -MP

ifeq ($(shell uname -s),Darwin)
ifneq ($(wildcard /opt/homebrew/bin/sdl2-config),)
SDL_FLAGS := -I/opt/homebrew/include $(shell /opt/homebrew/bin/sdl2-config --cflags)
SDL_LIBS := $(shell /opt/homebrew/bin/sdl2-config --libs) \
	-L/opt/homebrew/lib -Wl,-rpath,/opt/homebrew/lib -lSDL2_image
ifneq ($(wildcard /opt/homebrew/lib/libSDL2_mixer.dylib),)
CPPFLAGS += -DGAME_HAS_AUDIO=1
SDL_LIBS += -lSDL2_mixer
else
CPPFLAGS += -DGAME_HAS_AUDIO=0
endif
else
SDL_FLAGS := -F/Library/Frameworks
SDL_LIBS := -F/Library/Frameworks -Wl,-rpath,/Library/Frameworks \
	-framework SDL2 -framework SDL2_image
ifneq ($(wildcard /Library/Frameworks/SDL2_mixer.framework),)
CPPFLAGS += -DGAME_HAS_AUDIO=1
SDL_LIBS += -framework SDL2_mixer
else
CPPFLAGS += -DGAME_HAS_AUDIO=0
endif
endif
else
SDL_FLAGS := $(shell pkg-config --cflags sdl2 SDL2_image)
SDL_LIBS := $(shell pkg-config --libs sdl2 SDL2_image)
ifeq ($(shell pkg-config --exists SDL2_mixer && echo yes),yes)
CPPFLAGS += -DGAME_HAS_AUDIO=1
SDL_FLAGS += $(shell pkg-config --cflags SDL2_mixer)
SDL_LIBS += $(shell pkg-config --libs SDL2_mixer)
else
CPPFLAGS += -DGAME_HAS_AUDIO=0
endif
endif

.PHONY: all run test benchmark clean

all: $(TARGET)

$(TARGET): $(GAME_OBJECTS) .build/assets.stamp | build
	$(CXX) $(GAME_OBJECTS) $(SDL_LIBS) -o $@

$(TEST_TARGET): $(TEST_OBJECT) $(LOGIC_OBJECTS) | build
	$(CXX) $(TEST_OBJECT) $(LOGIC_OBJECTS) -o $@

$(BENCHMARK_TARGET): $(BENCHMARK_OBJECT) $(LOGIC_OBJECTS) | build
	$(CXX) $(BENCHMARK_OBJECT) $(LOGIC_OBJECTS) -o $@

.build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SDL_FLAGS) -c $< -o $@

.build/tests/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

.build/assets.stamp: $(shell find assets -type f -print)
	@mkdir -p .build build/assets
	cp -R assets/. build/assets/
	@touch $@

build:
	mkdir -p $@

run: all
	./$(TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

benchmark: $(BENCHMARK_TARGET)
	./$(BENCHMARK_TARGET)

clean:
	rm -rf .build build

-include $(DEPENDENCIES)
