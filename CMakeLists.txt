# Dùng khi bạn thích CMake (Windows/Visual Studio, CLion). Người dùng Linux/macOS có thể dùng Makefile.
cmake_minimum_required(VERSION 3.14)
project(edge_dp_scheduler CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

file(GLOB LIB_SRC src/*.cpp)
list(REMOVE_ITEM LIB_SRC
     ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/main_heuristics.cpp)

add_library(edgedp_lib ${LIB_SRC})
target_include_directories(edgedp_lib PUBLIC include src)

add_executable(edgedp_heur src/main_heuristics.cpp)
target_link_libraries(edgedp_heur edgedp_lib)

add_executable(edgedp src/main.cpp)
target_link_libraries(edgedp edgedp_lib)

add_executable(run_bench bench/run_bench.cpp src/json.cpp)
target_include_directories(run_bench PRIVATE src)
if(WIN32)
  target_link_libraries(run_bench psapi)
endif()

add_executable(brute_force_exec tests/brute_force_exec.cpp)
target_link_libraries(brute_force_exec edgedp_lib)

enable_testing()
add_executable(run_tests tests/test_main.cpp)
target_link_libraries(run_tests edgedp_lib)
add_test(NAME unit COMMAND run_tests)
