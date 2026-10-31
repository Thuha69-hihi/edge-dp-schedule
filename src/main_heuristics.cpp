// CLI test heuristic:
//   ./build/edgedp_heur --input data/x.json [--input data/y.json ...]
//   ./build/edgedp_heur --dir data/synthetic [--dp-max-n 2000]
// Với mỗi instance (sinh bởi scripts/gen_data.py): chạy 4 heuristic, và chạy DP baseline
// (nếu n <= --dp-max-n vì baseline là O(M*N^2)) để tính Energy Saved % = (E_heur - E_dp) / E_heur * 100.
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "edgedp/heuristics.hpp"
#include "edgedp/solver.hpp"
#include "io.hpp"

using namespace edgedp;
namespace fs = std::filesystem;

static double ms_since(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

int main(int argc, char** argv) {
    std::vector<std::string> files;
    std::string dir;
    int dp_max_n = 2000;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--input") && i + 1 < argc)
            files.push_back(argv[++i]);
        else if (!std::strcmp(argv[i], "--dir") && i + 1 < argc)
            dir = argv[++i];
        else if (!std::strcmp(argv[i], "--dp-max-n") && i + 1 < argc)
            dp_max_n = std::atoi(argv[++i]);
        else {
            std::cerr << "Tham so khong hop le: " << argv[i] << "\n";
            return 2;
        }
    }
    if (!dir.empty()) {
        std::error_code ec;
        if (!fs::is_directory(dir, ec)) {
            std::cerr << "Thu muc khong ton tai hoac khong hop le: " << dir << "\n";
            return 2;
        }
        for (const auto& e : fs::directory_iterator(dir, ec))
            if (e.is_regular_file(ec) && e.path().extension() == ".json") files.push_back(e.path().string());
    }
    if (files.empty()) {
        std::cerr << "Dung: edgedp_heur (--input <file.json> ... | --dir <thu_muc>) [--dp-max-n N]\n";
        return 2;
    }
    std::sort(files.begin(), files.end());

    std::cout << std::fixed << std::setprecision(4);
    for (const auto& path : files) {
        Instance in;
        try {
            std::ifstream f(path);
            if (!f) throw std::runtime_error("khong mo duoc file");
            in = read_instance_json(f);
        } catch (const std::exception& e) {
            std::cerr << "[bo qua] " << path << ": " << e.what() << "\n";
            continue;
        }

        // DP baseline làm mốc tối ưu (chỉ khi n đủ nhỏ).
        bool have_dp = false;
        Solution dp;
        if (in.n <= dp_max_n) {
            auto t0 = std::chrono::steady_clock::now();
            dp = solve_baseline(in);
            dp.time_ms = ms_since(t0);
            have_dp = dp.feasible;
        }

        std::cout << "\n== " << fs::path(path).filename().string() << "  (n=" << in.n << ", m=" << in.m
                  << ", seed=" << in.seed << ")\n";
        if (have_dp)
            std::cout << "DP baseline: energy=" << std::setprecision(6) << (double)dp.energy
                      << " nodes=" << dp.nodes_used << " time=" << std::setprecision(2) << dp.time_ms
                      << " ms\n";
        else if (in.n <= dp_max_n)
            std::cout << "DP baseline: khong kha thi\n";
        else
            std::cout << "DP baseline: bo qua (n > " << dp_max_n << ")\n";

        std::cout << std::left << std::setw(12) << "heuristic" << std::right << std::setw(16) << "energy"
                  << std::setw(7) << "nodes" << std::setw(6) << "feas" << std::setw(8) << "v_cap"
                  << std::setw(8) << "v_link" << std::setw(10) << "ms" << std::setw(12) << "saved%" << "\n";

        for (const auto& name : heuristic_names()) {
            auto t0 = std::chrono::steady_clock::now();
            HeuristicResult r = solve_heuristic(name, in);
            double t = ms_since(t0);
            std::cout << std::left << std::setw(12) << r.name << std::right << std::setw(16)
                      << std::setprecision(6) << (double)r.energy << std::setw(7) << r.nodes_used
                      << std::setw(6) << (r.feasible ? "yes" : "no") << std::setw(8) << r.violated_cap
                      << std::setw(8) << r.violated_link << std::setw(10) << std::setprecision(3) << t;
            if (have_dp && r.energy > 0)
                std::cout << std::setw(12) << std::setprecision(2)
                          << (double)((r.energy - dp.energy) / r.energy * 100.0);
            else
                std::cout << std::setw(12) << "-";
            std::cout << "\n";
        }
    }
    return 0;
}
