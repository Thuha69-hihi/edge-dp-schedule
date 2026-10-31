// Bộ test tối giản, không cần thư viện ngoài. Chạy: make test
// Kết quả: PASS / FAIL / SKIP (SKIP = solver chưa cài đặt).
#include <cmath>
#include <functional>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "edgedp/cost.hpp"
#include "edgedp/heuristics.hpp"
#include "edgedp/solver.hpp"
#include "io.hpp"
#include "json.hpp"

using namespace edgedp;

static int g_pass = 0, g_fail = 0, g_skip = 0;

#define CHECK(cond)                                                                               \
    do {                                                                                          \
        if (!(cond)) {                                                                            \
            std::cerr << "    CHECK failed: " #cond " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            return false;                                                                         \
        }                                                                                         \
    } while (0)

static bool close_enough(ld a, ld b) {
    return std::fabs(static_cast<double>(a - b)) <=
           1e-9 * std::max<double>(1.0, std::fabs(static_cast<double>(a)));
}

// Ví dụ ở mục 6 của tài liệu mô hình (đáp án: 973, dùng 2 node, cắt sau τ2).
static Instance example_n5() {
    Instance in;
    in.n = 5;
    in.m = 3;
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

#if defined(_MSC_VER)
#include <intrin.h>
static inline int popcount32(unsigned int x) {
    return static_cast<int>(__popcnt(x));
}
#else
static inline int popcount32(unsigned int x) {
    return __builtin_popcount(x);
}
#endif

// "Oracle" độc lập: duyệt mọi tập điểm cắt (N nhỏ). Trả +inf nếu vô nghiệm.
static ld oracle_enumerate(const Instance& in) {
    CostModel cm(in);
    const int n = in.n;
    ld best = inf();
    for (unsigned mask = 0; mask < (1u << (n - 1)); ++mask) {
        if (popcount32(mask) + 1 > in.m) continue;
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

static Instance random_instance(std::mt19937& rng, int maxN, int maxM,
                                EnergyModel model = EnergyModel::Cubic) {
    auto U = [&](int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng); };
    Instance in;
    in.n = U(1, maxN);
    in.m = U(1, maxM);
    for (int i = 0; i < in.n; ++i) {
        in.w.push_back(U(0, 8));
        in.d.push_back(U(0, 8));
    }
    const double kap[] = {0.5, 1.0, 2.0}, eps[] = {0.5, 1.0, 2.0};
    in.params.kappa_over_T2 = kap[U(0, 2)];
    in.params.eps = eps[U(0, 2)];
    in.params.e_on = U(0, 300);
    in.params.cap = U(8, 30);
    in.params.link_cap = U(3, 10);
    if (model == EnergyModel::Quadratic) {
        // a = 0 (tuyến tính) cũng là trường hợp biên: mọi đường thẳng cùng hệ số góc.
        const double qa[] = {0.0, 0.5, 1.0, 2.0}, qb[] = {0.0, 1.0, 3.0};
        in.params.model = EnergyModel::Quadratic;
        in.params.quad_a = qa[U(0, 3)];
        in.params.quad_b = qb[U(0, 2)];
        if (U(0, 2) == 0) in.params.cap = 1e18L;  // cửa sổ không bao giờ cắt: chạy nhánh deque của CHT
    }
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
    CHECK(cm.cost(0, 4) == inf());  // W = 10 > cap = 8
    CHECK(cm.cost(3, 5) == inf());  // cắt sau τ3: d_3 = 6 > link_cap = 5
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
    in.n = 1;
    in.m = 1;
    in.w = {5};
    in.d = {7};
    in.params = {2.0L, 3.0L, 0.5L, 10.0L, 1.0L};  // κ/T²=2, E_on=3, ε=0.5, cap=10, link_cap=1
    Solution s = solve_baseline(in);
    // k=0 nên d_0 = 7 không bị link_cap chặn: 3 + 2*125 + 0.5*7 = 256.5
    CHECK(s.feasible && close_enough(s.energy, 256.5) && s.nodes_used == 1 && s.cuts.empty());
    return true;
}

static bool empty_instance() {
    Instance in;
    in.n = 0;
    in.m = 3;
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
    in.n = 3;
    in.m = 2;
    in.w = {0, 0, 0};
    in.d = {0, 0, 0};
    in.params = {1.0L, 5.0L, 1.0L, 1.0L, 1.0L};
    Solution s = solve_baseline(in);
    CHECK(s.feasible && close_enough(s.energy, 5) && s.nodes_used == 1);  // một đoạn: chỉ tốn E_on
    return true;
}

static bool baseline_vs_oracle_random() {
    std::mt19937 rng(20261005);
    int feasible_cnt = 0, infeasible_cnt = 0;
    for (int t = 0; t < 1000; ++t) {
        Instance in = random_instance(rng, 11, 5);
        Solution s = solve_baseline(in);
        const ld ref = oracle_enumerate(in);
        CHECK((ref != inf()) == s.feasible);
        if (s.feasible) {
            CHECK(close_enough(ref, s.energy));
            ++feasible_cnt;
        } else {
            ++infeasible_cnt;
        }
        CHECK(valid_solution(in, s));
    }
    CHECK(feasible_cnt > 100 && infeasible_cnt > 100);  // phải có cả hai loại input (1000 vòng)
    return true;
}

// ---------------- Test mô hình bậc hai (model = quadratic) ----------------

static bool cost_quadratic_matches_formula() {
    Instance in;
    in.n = 2;
    in.m = 2;
    in.w = {2, 3};
    in.d = {1, 4};
    in.params.e_on = 10;
    in.params.eps = 0.5L;
    in.params.cap = 100;
    in.params.link_cap = 100;
    in.params.model = EnergyModel::Quadratic;
    in.params.quad_a = 2;
    in.params.quad_b = 1;
    CostModel cm(in);
    CHECK(close_enough(cm.cost(0, 2), 65.5L));  // 10 + 2*25 + 1*5 + 0.5*d_0(=1)
    CHECK(close_enough(cm.cost(1, 2), 33));     // 10 + 2*9  + 1*3 + 0.5*d_1(=4)
    in.params.model = EnergyModel::Cubic;       // mặc định cũ không đổi: kappa/T^2 * W^3
    in.params.kappa_over_T2 = 1;
    CHECK(close_enough(CostModel(in).cost(1, 2), 10 + 27 + 2));
    return true;
}

static bool json_parses_model() {
    const std::string base = R"({"n": 2, "m": 2, "w": [1, 2], "d": [1, 1],
      "params": {"kappa_over_T2": 3.0, "e_on": 1.0, "eps": 1.0, "cap": 9.0, "link_cap": 9.0)";
    Instance a = parse_instance_json(base + "}}");
    CHECK(a.params.model == EnergyModel::Cubic);  // không ghi model -> cubic
    Instance b = parse_instance_json(base + R"(, "model": "quadratic"}})");
    CHECK(b.params.model == EnergyModel::Quadratic && b.params.quad_a == 3.0L &&
          b.params.quad_b == 0);  // a mặc định = κ/T²
    Instance c = parse_instance_json(base + R"(, "model": "quadratic", "quad_a": 0.5, "quad_b": 2}})");
    CHECK(c.params.quad_a == 0.5L && c.params.quad_b == 2.0L);
    bool threw = false;
    try {
        parse_instance_json(base + R"(, "model": "linear"}})");
    } catch (const std::exception&) { threw = true; }
    CHECK(threw);
    threw = false;
    try {
        parse_instance_json(base + R"(, "model": "quadratic", "quad_a": -1}})");
    } catch (const std::exception&) { threw = true; }
    CHECK(threw);
    return true;
}

static bool cht_rejects_cubic() {
    bool threw = false;
    try {
        solve_cht(example_n5());
    } catch (const NotImplemented&) { threw = true; }
    CHECK(threw);  // CHT chỉ đúng cho bậc hai, không được âm thầm cho đáp án sai
    return true;
}

// Hai nhánh của CHT trên input lớn hơn: có cửa sổ cap (cây bao lồi) và không có (deque), so với baseline.
static bool cht_larger_inputs() {
    std::mt19937 rng(77);
    auto U = [&](int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng); };
    for (int t = 0; t < 120; ++t) {
        Instance in;
        in.n = U(100, 400);
        in.m = U(1, 8);
        ll tot = 0;
        for (int i = 0; i < in.n; ++i) {
            in.w.push_back(U(0, 20));
            in.d.push_back(U(0, 10));
            tot += in.w.back();
        }
        in.params.model = EnergyModel::Quadratic;
        in.params.quad_a = U(0, 3) * 0.5L;
        in.params.quad_b = U(0, 4);
        in.params.e_on = U(0, 5000);
        in.params.eps = U(0, 3) * 0.5L;
        in.params.link_cap = U(4, 11);
        const int mode = t % 3;  // 0: cap chặt, 1: cap vừa, 2: không cắt cửa sổ
        const ll per = tot / std::max(1, in.m);
        in.params.cap = (mode == 0) ? per + 25 : (mode == 1) ? 2 * per + 25 : tot + 1;
        Solution ref = solve_baseline(in), got = solve_cht(in);
        CHECK(got.feasible == ref.feasible);
        if (ref.feasible) CHECK(close_enough(got.energy, ref.energy));
        CHECK(valid_solution(in, got));
    }
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
    try {
        parse_instance_json(R"({"n": 2, "m": 1, "w": [1], "d": [1, 1], "params": {}})");
    } catch (const std::exception&) { threw = true; }
    CHECK(threw);
    threw = false;
    try {
        parse_instance_json("{not json");
    } catch (const std::exception&) { threw = true; }
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


// ---------------- Test biên bổ sung (việc 7.02) ----------------

// Bản bậc hai của ví dụ chuẩn (a = 1, b = 0): mọi solver chính xác cho 579.
static Instance example_n5_quadratic() {
    Instance in = example_n5();
    in.params.model = EnergyModel::Quadratic;
    in.params.quad_a = 1.0;
    in.params.quad_b = 0.0;
    return in;
}

// N = 0 và M = 0 cho MỌI solver (trước đây chỉ kiểm baseline).
static bool all_solvers_empty_and_no_nodes() {
    for (const auto& sv : all_solvers()) {
        Instance in = example_n5_quadratic();  // bậc hai để cả cht chạy được
        in.n = 0;
        in.w.clear();
        in.d.clear();
        Solution s = sv.fn(in);
        CHECK(s.feasible && s.energy == 0 && s.nodes_used == 0 && s.segments.empty());
        Instance in2 = example_n5_quadratic();
        in2.m = 0;
        CHECK(!sv.fn(in2).feasible);
    }
    return true;
}

static bool example_quadratic_all_solvers() {
    for (const auto& sv : all_solvers()) {
        Solution s = sv.fn(example_n5_quadratic());
        CHECK(s.feasible && close_enough(s.energy, 579) && s.nodes_used == 2);
        CHECK(s.cuts == std::vector<int>({2}));
        CHECK(valid_solution(example_n5_quadratic(), s));
    }
    return true;
}

static bool bruteforce_rejects_large_n() {
    Instance in;
    in.n = 19;
    in.m = 2;
    in.w.assign(19, 1);
    in.d.assign(19, 0);
    in.params = {1.0L, 1.0L, 1.0L, 100.0L, 100.0L};
    bool threw = false;
    try {
        solve_bruteforce(in);
    } catch (const NotImplemented&) { threw = true; }  // bắt tại đây, nếu không run() tính là SKIP
    CHECK(threw);
    return true;
}

static bool cht_rejects_negative_a() {
    Instance in = example_n5_quadratic();
    in.params.quad_a = -1.0;
    bool threw = false;
    try {
        solve_cht(in);
    } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
    return true;
}

// Bốn heuristic trên ví dụ chuẩn (khớp bảng của edgedp_heur trong README).
static bool heuristics_example_values() {
    const Instance in = example_n5();
    HeuristicResult g = solve_greedy(in);
    CHECK(g.feasible && close_enough(g.energy, 973) && g.nodes_used == 2);
    HeuristicResult e = solve_equal_split(in);
    CHECK(!e.feasible && close_enough(e.energy, 1051) && e.nodes_used == 3 && e.violated_link == 1);
    HeuristicResult o = solve_single_node(in);
    CHECK(!o.feasible && close_enough(o.energy, 1981) && o.nodes_used == 1 && o.violated_cap == 1);
    HeuristicResult r = solve_round_robin(in);
    CHECK(!r.feasible && close_enough(r.energy, 1108) && r.nodes_used == 3 && r.cuts.empty());
    CHECK(heuristic_names().size() == 4);
    for (const auto& name : heuristic_names()) CHECK(solve_heuristic(name, in).name == name);
    bool threw = false;
    try {
        solve_heuristic("khong_co", in);
    } catch (const std::exception&) { threw = true; }
    CHECK(threw);
    return true;
}

// N = 0: mọi heuristic khả thi với năng lượng 0; M = 0 mà còn việc: không khả thi.
static bool heuristics_trivial_cases() {
    Instance empty = example_n5();
    empty.n = 0;
    empty.w.clear();
    empty.d.clear();
    Instance none = example_n5();
    none.m = 0;
    for (const auto& name : heuristic_names()) {
        HeuristicResult a = solve_heuristic(name, empty);
        CHECK(a.feasible && a.energy == 0 && a.nodes_used == 0);
        CHECK(!solve_heuristic(name, none).feasible);
    }
    return true;
}

// Heuristic nào khả thi cũng không được rẻ hơn nghiệm tối ưu, và năng lượng của heuristic
// liên tiếp phải bằng tổng chi phí các đoạn theo điểm cắt nó báo.
static bool heuristics_never_beat_optimum() {
    std::mt19937 rng(7020);
    for (int it = 0; it < 300; ++it) {
        Instance in;
        in.n = 1 + static_cast<int>(rng() % 30);
        in.m = 1 + static_cast<int>(rng() % 6);
        ll sum = 0;
        for (int i = 0; i < in.n; ++i) {
            in.w.push_back(1 + static_cast<ll>(rng() % 20));
            in.d.push_back(static_cast<ll>(rng() % 10));
            sum += in.w.back();
        }
        const ld cap = static_cast<ld>(sum) * (0.3L + (rng() % 100) / 100.0L);
        in.params = {1.0L, static_cast<ld>(rng() % 500), 1.0L, cap, static_cast<ld>(rng() % 10)};
        const Solution opt = solve_baseline(in);
        for (const auto& name : heuristic_names()) {
            const HeuristicResult h = solve_heuristic(name, in);
            // Round-robin không giao đoạn liên tiếp (ngoài mô hình pipeline) nên có thể rẻ hơn
            // nghiệm tối ưu của bài toán chia chuỗi; chỉ so ba heuristic liên tiếp.
            if (h.feasible && name != "roundrobin") {
                CHECK(opt.feasible);
                CHECK(h.energy >= opt.energy - 1e-9L * std::max<ld>(1, opt.energy));
            }
            if (name != "roundrobin" && h.feasible) {
                CostModel cm(in);
                ld total = 0;
                int prev = 0;
                std::vector<int> ends = h.cuts;
                ends.push_back(in.n);
                for (int c : ends) {
                    total += cm.cost(prev, c);
                    prev = c;
                }
                CHECK(close_enough(total, h.energy));
                CHECK(static_cast<int>(ends.size()) == h.nodes_used);
            }
        }
    }
    return true;
}

// Bộ đọc JSON: literal, mảng/đối tượng rỗng, escape, số âm và số mũ; các lỗi cú pháp.
static bool json_parser_edge_cases() {
    Json j = parse_json(R"( {"a": true, "b": false, "c": null, "e": [], "o": {},
                             "s": "x\n\t\"\\\/y", "x": -1.5e2, "arr": [1, [2, 3]]} )");
    CHECK(j.type == Json::Obj);
    CHECK(j.get("a")->type == Json::Bool && j.get("a")->b);
    CHECK(j.get("b")->type == Json::Bool && !j.get("b")->b);
    CHECK(j.get("c")->type == Json::Null);
    CHECK(j.get("e")->type == Json::Arr && j.get("e")->arr.empty());
    CHECK(j.get("o")->type == Json::Obj && j.get("o")->obj.empty());
    CHECK(j.get("s")->str == "x\n\t\"\\/y");
    CHECK(parse_json("\"a\\u00e9b\\r\\b\\f\"").str == "a?b\r\b\f");  // \u thay bằng '?' (không cần unicode)
    CHECK(close_enough(j.get("x")->num, -150));
    CHECK(j.get("arr")->arr.size() == 2 && j.get("arr")->arr[1].arr.size() == 2);
    CHECK(j.get("khong_co") == nullptr);
    for (const char* bad : {"[1, 2", "{\"a\" 1}", "\"chua dong", "\"\\", "tru", "1 2", "{\"a\": }", ""}) {
        bool threw = false;
        try {
            parse_json(bad);
        } catch (const std::exception&) { threw = true; }
        CHECK(threw);
    }
    return true;
}

// Đọc instance: từ chối số âm, số thực, phần tử không phải số, n/m âm.
static bool instance_json_rejects_invalid_values() {
    const char* bads[] = {
        R"({"n": 1, "m": 1, "w": [-1], "d": [0], "params": {"kappa_over_T2": 1, "e_on": 1, "eps": 1, "cap": 1, "link_cap": 1}})",
        R"({"n": 1, "m": 1, "w": [1.5], "d": [0], "params": {"kappa_over_T2": 1, "e_on": 1, "eps": 1, "cap": 1, "link_cap": 1}})",
        R"({"n": 1, "m": 1, "w": ["a"], "d": [0], "params": {"kappa_over_T2": 1, "e_on": 1, "eps": 1, "cap": 1, "link_cap": 1}})",
        R"({"n": 1, "m": -1, "w": [1], "d": [0], "params": {"kappa_over_T2": 1, "e_on": 1, "eps": 1, "cap": 1, "link_cap": 1}})",
        R"({"n": 1, "m": 1, "w": [1], "d": [0], "params": {"kappa_over_T2": 1, "e_on": 1, "eps": 1, "cap": 1}})",
        R"({"n": 2, "m": 1, "w": [1, 1], "d": [0], "params": {"kappa_over_T2": 1, "e_on": 1, "eps": 1, "cap": 9, "link_cap": 1}})",
    };
    for (const char* b : bads) {
        bool threw = false;
        try {
            parse_instance_json(b);
        } catch (const std::exception&) { threw = true; }
        CHECK(threw);
    }
    return true;
}

// ---------------- Đối chiếu solver khác với baseline (SKIP nếu chưa cài) ----------------

// Brute force là O(2^(N-1)) nên chỉ cho N <= 12 (việc 2.04 của TV4); D&C và CHT thử tới N = 40.
// model: cubic hoặc quadratic. CHT chỉ hỗ trợ quadratic: với cubic nó ném NotImplemented -> SKIP.
static bool cross_check(SolverFn fn, int max_n, EnergyModel model) {
    std::mt19937 rng(2026);
    for (int t = 0; t < 400; ++t) {
        Instance in = random_instance(rng, max_n, 5, model);
        Solution ref = solve_baseline(in);
        Solution got = fn(in);  // có thể ném NotImplemented -> SKIP
        CHECK(got.feasible == ref.feasible);
        if (ref.feasible) CHECK(close_enough(got.energy, ref.energy));  // so energy, không so cuts
        CHECK(valid_solution(in, got));
    }
    // Ví dụ trong tài liệu (cubic) phải ra 973.
    if (model == EnergyModel::Cubic) {
        Solution e = fn(example_n5());
        CHECK(e.feasible && close_enough(e.energy, 973));
    }
    return true;
}

static void run(const std::string& name, const std::function<bool()>& f) {
    try {
        if (f()) {
            ++g_pass;
            std::cout << "[PASS] " << name << "\n";
        } else {
            ++g_fail;
            std::cout << "[FAIL] " << name << "\n";
        }
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
    run("cost_quadratic_matches_formula", cost_quadratic_matches_formula);
    run("json_parses_model", json_parses_model);
    run("cht_rejects_cubic", cht_rejects_cubic);
    run("cht_larger_inputs", cht_larger_inputs);
    run("all_solvers_empty_and_no_nodes", all_solvers_empty_and_no_nodes);
    run("example_quadratic_all_solvers", example_quadratic_all_solvers);
    run("bruteforce_rejects_large_n", bruteforce_rejects_large_n);
    run("cht_rejects_negative_a", cht_rejects_negative_a);
    run("heuristics_example_values", heuristics_example_values);
    run("heuristics_trivial_cases", heuristics_trivial_cases);
    run("heuristics_never_beat_optimum", heuristics_never_beat_optimum);
    run("json_parser_edge_cases", json_parser_edge_cases);
    run("instance_json_rejects_invalid_values", instance_json_rejects_invalid_values);

    for (const auto& s : all_solvers()) {
        if (std::string(s.name) == "baseline") continue;
        const int max_n = std::string(s.name) == "bruteforce" ? 12 : 40;
        run(std::string("cross_check_") + s.name + "_vs_baseline",
            [&] { return cross_check(s.fn, max_n, EnergyModel::Cubic); });
        run(std::string("cross_check_") + s.name + "_vs_baseline_quadratic",
            [&] { return cross_check(s.fn, max_n, EnergyModel::Quadratic); });
    }

    std::cout << "\nTong ket: " << g_pass << " pass, " << g_fail << " fail, " << g_skip << " skip\n";
    return g_fail == 0 ? 0 : 1;
}
