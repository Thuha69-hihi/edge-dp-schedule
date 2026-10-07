CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Iinclude
LIB_SRC  := $(filter-out src/main.cpp,$(wildcard src/*.cpp))
BUILD    := build

all: $(BUILD)/edgedp $(BUILD)/run_tests

$(BUILD)/edgedp: $(wildcard src/*.cpp src/*.hpp include/edgedp/*.hpp)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) src/*.cpp -o $@

$(BUILD)/run_tests: $(LIB_SRC) tests/test_main.cpp $(wildcard src/*.hpp include/edgedp/*.hpp)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -Isrc $(LIB_SRC) tests/test_main.cpp -o $@

test: $(BUILD)/run_tests
	./$(BUILD)/run_tests

clean:
	rm -rf $(BUILD)

.PHONY: all test clean
