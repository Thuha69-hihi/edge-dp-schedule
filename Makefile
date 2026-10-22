CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Iinclude
LIB_SRC  := $(filter-out src/main.cpp src/main_heuristics.cpp,$(wildcard src/*.cpp))
BUILD    := build

all: $(BUILD)/edgedp $(BUILD)/edgedp_heur $(BUILD)/run_bench $(BUILD)/run_tests

$(BUILD)/edgedp: $(LIB_SRC) src/main.cpp $(wildcard src/*.hpp include/edgedp/*.hpp)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -Isrc $(LIB_SRC) src/main.cpp -o $@

$(BUILD)/edgedp_heur: $(LIB_SRC) src/main_heuristics.cpp $(wildcard src/*.hpp include/edgedp/*.hpp)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -Isrc $(LIB_SRC) src/main_heuristics.cpp -o $@

$(BUILD)/run_bench: bench/run_bench.cpp src/json.cpp src/json.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -Isrc bench/run_bench.cpp src/json.cpp -o $@

$(BUILD)/run_tests: $(LIB_SRC) tests/test_main.cpp $(wildcard src/*.hpp include/edgedp/*.hpp)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -Isrc $(LIB_SRC) tests/test_main.cpp -o $@

test: $(BUILD)/run_tests
	./$(BUILD)/run_tests

clean:
	rm -rf $(BUILD)

.PHONY: all test clean
