// Bộ test tối giản, không cần thư viện ngoài. Chạy: make test
// Kết quả: PASS / FAIL / SKIP (SKIP = solver chưa cài đặt).
#include <cmath>
#include <functional>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "edgedp/cost.hpp"
#include "edgedp/solver.hpp"
#include "io.hpp"

using namespace edgedp;

static int g_pass = 0, g_fail = 0, g_skip = 0;

#define CHECK(cond)                                                                           \
    do {                                                                                      \
        if (!(cond)) {                                                                        \
            std::cerr << "    CHECK failed: " #cond " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            return false;                                                                     \
        }                                                                                     \
    } while (0)

static bool close_enough(ld a, ld b) {
    return std::fabs(static_cast<double>(a - b)) <= 1e-9 * std::max<double>(1.0, std::fabs(static_cast<double>(a)));
}

// Ví dụ ở mục 6 của tài liệu mô hình (đáp án: 973, dùng 2 node, cắt sau τ2).
static Instance example_n5() {
    Instance in;
    in.n = 5; in.m = 3;
    in.w = {2, 3, 1, 4, 2};
    in.d = {3, 4, 2, 6, 1};
    in.params.kappa_over_T2 = 1.0;
    in.params.e_on = 250.0;
    in.params.eps = 1.0;
    in.params.cap = 8.0;
    in.params.link_cap = 5.0;
    return in;
}

// Kiểm tra nghiệm hợp lệ: đoạn phủ kín 1..N liên tiếp, <= M đoạn, mỗi đoạn hữu hạn,
// năng lượng bằng tổng chi phí các đoạn, cuts khớp với các đoạn.
static bool valid_solution(const Instance& in, const Solution& s) {
    if (!s.feasible) return true;
    CostModel cm(in);
    const int j = static_cast<int>(s.segments.size());
    if (j != s.nodes_used || j > in.m) return false;
    if (static_cast<int>(s.cuts.size()) != std::max(0, j - 1)) return false;
    int pos = 0;
    ld total = 0;
    for (int t = 0; t < j; ++t) {
        const Segment& g = s.segments[t];
        if (g.from != pos + 1 || g.to < g.from || g.to > in.n) return false;
        const ld c = cm.cost(pos, g.to);
        if (c == inf()) return false;
        if (!close_enough(c, g.cost) || g.work != cm.work(pos, g.to)) return false;
        total += c;
        pos = g.to;
        if (t + 1 < j && s.cuts[t] != g.to) return false;
    }
    return pos == in.n && close_enough(total, s.energy);
}

// "Oracle" độc lập: duyệt mọi tập điểm cắt (N nhỏ). Trả +inf nếu vô nghiệm.
static ld oracle_enumerate(const Instance& in) {
    CostModel cm(in);
    const int n = in.n;
    ld best = inf();
    for (unsigned mask = 0; mask < (1u << (n - 1)); ++mask) {
        if (__builtin_popcount(mask) + 1 > in.m) continue;
        int prev = 0;
        ld total = 0;
        for (int i = 1; i <= n && total != inf(); ++i) {
            const bool cut_here = (i == n) || ((mask >> (i - 1)) & 1u);
            if (!cut_here) continue;
            total += cm.cost(prev, i);
            prev = i;
        }
        if (total < best) best = total;
    }
    return best;
}

static Instance random_instance(std::mt19937& rng, int maxN, int maxM) {
    auto U = [&](int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng); };
    Instance in;
    in.n = U(1, maxN);
    in.m = U(1, maxM);
    for (int i = 0; i < in.n; ++i) { in.w.push_back(U(0, 8)); in.d.push_back(U(0, 8)); }
    const double kap[] = {0.5, 1.0, 2.0}, eps[] = {0.5, 1.0, 2.0};
    in.params.kappa_over_T2 = kap[U(0, 2)];
    in.params.eps = eps[U(0, 2)];
    in.params.e_on = U(0, 300);
    in.params.cap = U(8, 30);
    in.params.link_cap = U(3, 10);
    return in;
}

// ---------------- Test baseline ----------------

static bool example_matches_doc() {
    Instance in = example_n5();
    Solution s = solve_baseline(in);
    CHECK(s.feasible);
    CHECK(close_enough(s.energy, 973));
    CHECK(s.nodes_used == 2);
    CHECK(s.cuts.size() == 1 && s.cuts[0] == 2);
    CHECK(s.segments.size() == 2);
    CHECK(s.segments[0].from == 1 && s.segments[0].to == 2 && s.segments[0].work == 5);
    CHECK(close_enough(s.segments[0].cost, 378));
    CHECK(s.segments[1].from == 3 && s.segments[1].to == 5 && s.segments[1].work == 7);
    CHECK(close_enough(s.segments[1].cost, 595));
    CHECK(valid_solution(in, s));
    return true;
}

static bool example_costs_match_doc_table() {
    // Vài ô trong bảng DP của tài liệu: Cost(1,1)+... => DP(1,1)=261, DP(2,1)=378, DP(3,1)=469.
    Instance in = example_n5();
    CostModel cm(in);
    CHECK(close_enough(cm.cost(0, 1), 261));
    CHECK(close_enough(cm.cost(0, 2), 378));
    CHECK(close_enough(cm.cost(0, 3), 469));
    CHECK(cm.cost(0, 4) == inf());   // W = 10 > cap = 8
    CHECK(cm.cost(3, 5) == inf());   // cắt sau τ3: d_3 = 6 > link_cap = 5
    return true;
}

static bool example_single_node_infeasible() {
    Instance in = example_n5();
    in.m = 1;  // tổng W = 12 > 8
    Solution s = solve_baseline(in);
    CHECK(!s.feasible);
    return true;
}

static bool example_low_e_on_uses_three_nodes() {
    // Tài liệu: E_on dưới ~209 thì 3 node thành tối ưu. Với E_on = 200: 2 node = 873, 3 node = 864.
    Instance in = example_n5();
    in.params.e_on = 200;
    Solution s = solve_baseline(in);
    CHECK(s.feasible && s.nodes_used == 3 && close_enough(s.energy, 864));
    CHECK(s.cuts.size() == 2 && s.cuts[0] == 2 && s.cuts[1] == 4);
    return true;
}

static bool task_too_heavy_infeasible() {
    Instance in = example_n5();
    in.params.cap = 3;  // τ4 có w = 4 > 3, không đoạn nào chứa nổi
    Solution s = solve_baseline(in);
    CHECK(!s.feasible);
    return true;
}

static bool single_task() {
    Instance in;
    in.n = 1; in.m = 1;
    in.w = {5}; in.d = {7};
    in.params = {2.0L, 3.0L, 0.5L, 10.0L, 1.0L};  // κ/T²=2, E_on=3, ε=0.5, cap=10, link_cap=1
    Solution s = solve_baseline(in);
    // k=0 nên d_0 = 7 không bị link_cap chặn: 3 + 2*125 + 0.5*7 = 256.5
    CHECK(s.feasible && close_enough(s.energy, 256.5) && s.nodes_used == 1 && s.cuts.empty());
    return true;
}

static bool empty_instance() {
    Instance in;
    in.n = 0; in.m = 3;
    Solution s = solve_baseline(in);
    CHECK(s.feasible && s.energy == 0 && s.nodes_used == 0 && s.segments.empty());
    return true;
}

static bool no_nodes() {
    Instance in = example_n5();
    in.m = 0;
    CHECK(!solve_baseline(in).feasible);
    return true;
}

static bool zero_work_tasks() {
    Instance in;
    in.n = 3; in.m = 2;
    in.w = {0, 0, 0}; in.d = {0, 0, 0};
    in.params = {1.0L, 5.0L, 1.0L, 1.0L, 1.0L};
    Solution s = solve_baseline(in);
    CHECK(s.feasible && close_enough(s.energy, 5) && s.nodes_used == 1);  // một đoạn: chỉ tốn E_on
    return true;
}

static bool baseline_vs_oracle_random() {
    std::mt19937 rng(20261005);
    int feasible_cnt = 0, infeasible_cnt = 0;
    for (int t = 0; t < 600; ++t) {
        Instance in = random_instance(rng, 11, 5);
        Solution s = solve_baseline(in);
        const ld ref = oracle_enumerate(in);
        CHECK((ref != inf()) == s.feasible);
        if (s.feasible) { CHECK(close_enough(ref, s.energy)); ++feasible_cnt; } else { ++infeasible_cnt; }
        CHECK(valid_solution(in, s));
    }
    CHECK(feasible_cnt > 50 && infeasible_cnt > 50);  // phải có cả hai loại input
    return true;
}

// ---------------- Test I/O JSON ----------------

static bool json_roundtrip_example() {
    const std::string text = R"({
      "n": 5, "m": 3,
      "w": [2, 3, 1, 4, 2],
      "d": [3, 4, 2, 6, 1],
      "params": {"kappa_over_T2": 1.0, "e_on": 250.0, "eps": 1.0, "cap": 8.0, "link_cap": 5.0},
      "seed": 7, "source": "synthetic"
    })";
    Instance in = parse_instance_json(text);
    CHECK(in.n == 5 && in.m == 3 && in.w.size() == 5 && in.d.size() == 5);
    CHECK(in.seed == 7 && in.source == "synthetic");
    Solution s = solve_baseline(in);
    s.solver = "baseline";
    const std::string out = solution_to_json(s);
    CHECK(out.find("\"feasible\": true") != std::string::npos);
    CHECK(out.find("\"energy\": 973") != std::string::npos);
    CHECK(out.find("\"cuts\": [2]") != std::string::npos);
    return true;
}

static bool json_rejects_bad_input() {
    bool threw = false;
    try { parse_instance_json(R"({"n": 2, "m": 1, "w": [1], "d": [1, 1], "params": {}})"); }
    catch (const std::exception&) { threw = true; }
    CHECK(threw);
    threw = false;
    try { parse_instance_json("{not json"); } catch (const std::exception&) { threw = true; }
    CHECK(threw);
    return true;
}

static bool json_infeasible_prints_null() {
    Instance in = example_n5();
    in.m = 1;
    Solution s = solve_baseline(in);
    const std::string out = solution_to_json(s);
    CHECK(out.find("\"feasible\": false") != std::string::npos);
    CHECK(out.find("\"energy\": null") != std::string::npos);
    return true;
}

// ---------------- Đối chiếu solver khác với baseline (SKIP nếu chưa cài) ----------------

static bool cross_check(SolverFn fn) {
    std::mt19937 rng(2026);
    for (int t = 0; t < 400; ++t) {
        Instance in = random_instance(rng, 40, 6);
        Solution ref = solve_baseline(in);
        Solution got = fn(in);  // có thể ném NotImplemented -> SKIP
        CHECK(got.feasible == ref.feasible);
        if (ref.feasible) CHECK(close_enough(got.energy, ref.energy));  // so energy, không so cuts
        CHECK(valid_solution(in, got));
    }
    // Ví dụ trong tài liệu cũng phải ra 973.
    Solution e = fn(example_n5());
    CHECK(e.feasible && close_enough(e.energy, 973));
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
    run("example_matches_doc", example_matches_doc);
    run("example_costs_match_doc_table", example_costs_match_doc_table);
    run("example_single_node_infeasible", example_single_node_infeasible);
    run("example_low_e_on_uses_three_nodes", example_low_e_on_uses_three_nodes);
    run("task_too_heavy_infeasible", task_too_heavy_infeasible);
    run("single_task", single_task);
    run("empty_instance", empty_instance);
    run("no_nodes", no_nodes);
    run("zero_work_tasks", zero_work_tasks);
    run("baseline_vs_oracle_random", baseline_vs_oracle_random);
    run("json_roundtrip_example", json_roundtrip_example);
    run("json_rejects_bad_input", json_rejects_bad_input);
    run("json_infeasible_prints_null", json_infeasible_prints_null);

    for (const auto& s : all_solvers()) {
        if (std::string(s.name) == "baseline") continue;
        run(std::string("cross_check_") + s.name + "_vs_baseline", [&] { return cross_check(s.fn); });
    }

    std::cout << "\nTong ket: " << g_pass << " pass, " << g_fail << " fail, " << g_skip << " skip\n";
    return g_fail == 0 ? 0 : 1;
}
