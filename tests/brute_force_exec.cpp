// Brute force độc lập cho N nhỏ (N <= 18), dùng làm chuẩn đối chiếu với baseline.
// Biên dịch:
//   make build/brute_force_exec
// Sử dụng:
//   ./build/brute_force_exec tests/data/example_n5.json
//   ./build/brute_force_exec path/to/instance.json
//
// Đầu vào: JSON định dạng như CLI edgedp.
// Đầu ra: JSON cùng định dạng output của edgedp (bao gồm energy + cuts).
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>

#include "edgedp/solver.hpp"
#include "io.hpp"

using namespace edgedp;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Dung: brute_force <instance.json>\n";
        return 2;
    }
    Instance in;
    try {
        std::ifstream f(argv[1]);
        if (!f) throw std::runtime_error("khong mo duoc file: " + std::string(argv[1]));
        in = read_instance_json(f);
    } catch (const std::exception& e) {
        std::cerr << "Loi input: " << e.what() << "\n";
        return 2;
    }

    auto t0 = std::chrono::steady_clock::now();
    Solution sol;
    try {
        sol = solve_bruteforce(in);
    } catch (const NotImplemented& e) {
        std::cerr << "brute_force khong chay duoc: " << e.what() << "\n";
        return 3;
    }
    auto t1 = std::chrono::steady_clock::now();
    sol.solver = "bruteforce";
    sol.time_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::cout << solution_to_json(sol);
    return sol.feasible ? 0 : 4;
}
