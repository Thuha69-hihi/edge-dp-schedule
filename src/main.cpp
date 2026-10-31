// CLI:  ./build/edgedp --algo baseline --input tests/data/example_n5.json
// In kết quả JSON (mục 8 tài liệu mô hình) ra stdout.
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

#include "edgedp/solver.hpp"
#include "io.hpp"

using namespace edgedp;

int main(int argc, char** argv) {
    std::string algo = "baseline", path;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--algo") && i + 1 < argc)
            algo = argv[++i];
        else if (!std::strcmp(argv[i], "--input") && i + 1 < argc)
            path = argv[++i];
        else {
            std::cerr << "Tham so khong hop le: " << argv[i] << "\n";
            return 2;
        }
    }
    if (path.empty()) {
        std::cerr << "Dung: edgedp --algo <baseline|cht|dc|bruteforce> --input <file.json>\n";
        return 2;
    }

    Instance in;
    try {
        std::ifstream f(path);
        if (!f) throw std::runtime_error("khong mo duoc file: " + path);
        in = read_instance_json(f);
    } catch (const std::exception& e) {
        std::cerr << "Loi input: " << e.what() << "\n";
        return 2;
    }

    for (const auto& s : all_solvers()) {
        if (algo != s.name) continue;
        try {
            auto t0 = std::chrono::steady_clock::now();
            Solution sol = s.fn(in);
            auto t1 = std::chrono::steady_clock::now();
            sol.solver = algo;
            sol.time_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            std::cout << solution_to_json(sol);
            return 0;
        } catch (const NotImplemented& e) {
            std::cerr << e.what() << "\n";
            return 3;
        }
    }
    std::cerr << "Khong co thuat toan: " << algo << "\n";
    return 2;
}
