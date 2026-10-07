// CLI:  ./build/edgedp --algo baseline --input data/sample.txt [--show]
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
    bool show = false;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--algo") && i + 1 < argc) algo = argv[++i];
        else if (!std::strcmp(argv[i], "--input") && i + 1 < argc) path = argv[++i];
        else if (!std::strcmp(argv[i], "--show")) show = true;
        else { std::cerr << "Tham so khong hop le: " << argv[i] << "\n"; return 2; }
    }
    if (path.empty()) {
        std::cerr << "Dung: edgedp --algo <baseline|cht|dc|bruteforce> --input <file> [--show]\n";
        return 2;
    }
    std::ifstream f(path);
    Instance in;
    if (!f || !read_instance(f, in)) { std::cerr << "Khong doc duoc input: " << path << "\n"; return 2; }

    for (const auto& s : all_solvers()) {
        if (algo != s.name) continue;
        try {
            auto t0 = std::chrono::steady_clock::now();
            Solution sol = s.fn(in);
            auto t1 = std::chrono::steady_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            std::cout << "algo=" << algo << " N=" << in.work.size() << " M=" << in.nodes.size()
                      << " feasible=" << sol.feasible << " energy=" << sol.energy
                      << " time_ms=" << ms << "\n";
            if (show)
                for (const auto& b : sol.batches)
                    std::cout << "  batch [" << b.begin << ", " << b.end << ") -> node " << b.node << "\n";
            return 0;
        } catch (const NotImplemented& e) {
            std::cerr << e.what() << "\n";
            return 3;
        }
    }
    std::cerr << "Khong co thuat toan: " << algo << "\n";
    return 2;
}
