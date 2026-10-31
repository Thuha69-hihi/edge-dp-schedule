CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Iinclude
LIB_SRC  := $(filter-out src/main.cpp src/main_heuristics.cpp,$(wildcard src/*.cpp))
BUILD    := build

all: $(BUILD)/edgedp $(BUILD)/edgedp_heur $(BUILD)/run_bench $(BUILD)/run_tests $(BUILD)/brute_force_exec

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

$(BUILD)/brute_force_exec: $(LIB_SRC) tests/brute_force_exec.cpp $(wildcard src/*.hpp include/edgedp/*.hpp)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -Isrc $(LIB_SRC) tests/brute_force_exec.cpp -o $@

brute: $(BUILD)/brute_force_exec
	./$(BUILD)/brute_force_exec tests/data/example_n5.json

test: $(BUILD)/run_tests
	./$(BUILD)/run_tests

# Tái tạo toàn bộ thí nghiệm (xem README, mục 'Tái tạo thí nghiệm')
reproduce:
	bash scripts/reproduce.sh

reproduce-quick:
	bash scripts/reproduce.sh --quick

# Đo độ phủ (việc 7.02): build lại với --coverage, chạy make test + pytest, in báo cáo gcovr.
# Cần gcovr (pip install gcovr). Xong thì "make clean && make" để quay lại bản -O2.
COV_FILTER := --filter src/ --filter include/ --exclude src/main_heuristics.cpp
coverage:
	rm -rf $(BUILD)
	$(MAKE) all CXXFLAGS="-std=c++17 -O0 -g --coverage -Wall -Wextra -Iinclude"
	./$(BUILD)/run_tests
	python3 -m pytest tests/unittest2 -q
	gcovr -r . $(COV_FILTER)
	gcovr -r . $(COV_FILTER) --txt-metric branch --exclude-throw-branches --exclude-unreachable-branches

clean:
	rm -rf $(BUILD)

.PHONY: all test clean brute reproduce reproduce-quick coverage
