// Bộ test tối giản, không cần thư viện ngoài. Chạy: make test
// Quy ước kết quả: PASS / FAIL / SKIP (SKIP = solver chưa cài đặt).
#include <functional>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "edgedp/solver.hpp"

using namespace edgedp;

static int g_pass = 0, g_fail = 0, g_skip = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::cerr << "    CHECK failed: " #cond " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            return false;                                                        \
        }                                                                        \
    } while (0)

// Tính lại năng lượng từ các batch và kiểm tra các batch phủ kín [0, N) liên tiếp.
static bool valid_solution(const Instance& in, const Solution& s) {
    const int n = static_cast<int>(in.work.size());
    int pos = 0;
    ll e = 0;
    for (const auto& b : s.batches) {
        if (b.begin != pos || b.end <= b.begin || b.end > n) return false;
        if (b.node < 0 || b.node >= static_cast<int>(in.nodes.size())) return false;
        ll w = 0;
        for (int i = b.begin; i < b.end; ++i) w += in.work[i];
        e += in.nodes[b.node].alpha * w * w + in.nodes[b.node].setup;
        pos = b.end;
    }
    return pos == n && e == s.energy;
}

static Instance random_instance(std::mt19937& rng, int maxN, int maxM) {
    Instance in;
    int n = std::uniform_int_distribution<int>(0, maxN)(rng);
    int m = std::uniform_int_distribution<int>(1, maxM)(rng);
    for (int i = 0; i < n; ++i) in.work.push_back(std::uniform_int_distribution<int>(1, 20)(rng));
    for (int k = 0; k < m; ++k)
        in.nodes.push_back({std::uniform_int_distribution<int>(1, 5)(rng),
                            std::uniform_int_distribution<int>(0, 200)(rng)});
    return in;
}

// ---- Test cho baseline: giá trị tính tay ----
static bool baseline_empty() {
    Solution s = solve_baseline(Instance{{}, {{1, 10}}});
    CHECK(s.feasible && s.energy == 0 && s.batches.empty());
    return true;
}
static bool baseline_single_task() {
    Solution s = solve_baseline(Instance{{5}, {{2, 3}}});
    CHECK(s.feasible && s.energy == 2 * 25 + 3);
    CHECK(s.batches.size() == 1);
    return true;
}
static bool baseline_hand_computed() {
    // work = 1 2 3, alpha=1, setup=10. Tốt nhất: {1,2} + {3} = (9+10) + (9+10) = 38
    Instance in{{1, 2, 3}, {{1, 10}}};
    Solution s = solve_baseline(in);
    CHECK(s.feasible && s.energy == 38);
    CHECK(s.batches.size() == 2);
    CHECK(s.batches[0].begin == 0 && s.batches[0].end == 2);
    CHECK(valid_solution(in, s));
    return true;
}
static bool baseline_no_nodes() {
    Solution s = solve_baseline(Instance{{1, 2}, {}});
    CHECK(!s.feasible);
    return true;
}
static bool baseline_picks_cheaper_node() {
    // node0: 1*16+100 = 116; node1: 2*16+0 = 32  -> chọn node 1
    Solution s = solve_baseline(Instance{{4}, {{1, 100}, {2, 0}}});
    CHECK(s.energy == 32 && s.batches.size() == 1 && s.batches[0].node == 1);
    return true;
}
static bool baseline_equal_tasks() {
    // setup=0 -> tách hết thành từng tác vụ là tốt nhất: 4 * 1 = 4
    Instance in{{1, 1, 1, 1}, {{1, 0}}};
    Solution s = solve_baseline(in);
    CHECK(s.energy == 4 && s.batches.size() == 4);
    return true;
}
static bool baseline_random_valid() {
    std::mt19937 rng(12345);
    for (int t = 0; t < 200; ++t) {
        Instance in = random_instance(rng, 30, 4);
        Solution s = solve_baseline(in);
        CHECK(s.feasible);
        CHECK(valid_solution(in, s));
    }
    return true;
}

// ---- Đối chiếu các solver khác với baseline (SKIP nếu chưa cài) ----
static bool cross_check(SolverFn fn) {
    std::mt19937 rng(2026);
    for (int t = 0; t < 300; ++t) {
        Instance in = random_instance(rng, 40, 4);
        Solution ref = solve_baseline(in);
        Solution got = fn(in);  // có thể ném NotImplemented -> SKIP
        CHECK(got.feasible == ref.feasible);
        CHECK(got.energy == ref.energy);
        CHECK(valid_solution(in, got));
    }
    return true;
}

static void run(const std::string& name, const std::function<bool()>& f) {
    try {
        if (f()) { ++g_pass; std::cout << "[PASS] " << name << "\n"; }
        else { ++g_fail; std::cout << "[FAIL] " << name << "\n"; }
    } catch (const NotImplemented& e) {
        ++g_skip;
        std::cout << "[SKIP] " << name << "  (" << e.what() << ")\n";
    } catch (const std::exception& e) {
        ++g_fail;
        std::cout << "[FAIL] " << name << "  exception: " << e.what() << "\n";
    }
}

int main() {
    run("baseline_empty", baseline_empty);
    run("baseline_single_task", baseline_single_task);
    run("baseline_hand_computed", baseline_hand_computed);
    run("baseline_no_nodes", baseline_no_nodes);
    run("baseline_picks_cheaper_node", baseline_picks_cheaper_node);
    run("baseline_equal_tasks", baseline_equal_tasks);
    run("baseline_random_valid", baseline_random_valid);

    for (const auto& s : all_solvers()) {
        if (std::string(s.name) == "baseline") continue;
        run(std::string("cross_check_") + s.name + "_vs_baseline", [&] { return cross_check(s.fn); });
    }

    std::cout << "\nTong ket: " << g_pass << " pass, " << g_fail << " fail, " << g_skip << " skip\n";
    return g_fail == 0 ? 0 : 1;
}
