CXX ?= c++
CPPFLAGS := -Iinclude
CXXFLAGS ?= -std=c++20 -O3 -Wall -Wextra -Wpedantic -Werror -pthread
LDFLAGS ?= -pthread

BUILD_DIR := build
LIB_OBJECTS := $(BUILD_DIR)/blur.o $(BUILD_DIR)/pgm.o $(BUILD_DIR)/statistics.o
APP_OBJECTS := $(LIB_OBJECTS) $(BUILD_DIR)/main.o
TEST_OBJECTS := $(LIB_OBJECTS) $(BUILD_DIR)/test_main.o
DEPENDENCIES := $(APP_OBJECTS:.o=.d) $(BUILD_DIR)/test_main.d

.PHONY: all test benchmark-smoke crossover-smoke clean

all: $(BUILD_DIR)/parallel-pixels

test: $(BUILD_DIR)/parallel-pixels-tests
	./$(BUILD_DIR)/parallel-pixels-tests

benchmark-smoke: $(BUILD_DIR)/parallel-pixels
	./$(BUILD_DIR)/parallel-pixels benchmark --width 128 --height 128 --threads 4 --iterations 3 --json

crossover-smoke: $(BUILD_DIR)/parallel-pixels
	SIZES="16 32" THREADS="1 2" ITERATIONS=1 ./tools/crossover_sweep.sh

$(BUILD_DIR)/parallel-pixels: $(APP_OBJECTS)
	$(CXX) $(APP_OBJECTS) $(LDFLAGS) -o $@

$(BUILD_DIR)/parallel-pixels-tests: $(TEST_OBJECTS)
	$(CXX) $(TEST_OBJECTS) $(LDFLAGS) -o $@

$(BUILD_DIR)/blur.o: src/blur.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/pgm.o: src/pgm.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/statistics.o: src/statistics.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/main.o: src/main.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/test_main.o: tests/test_main.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPENDENCIES)
