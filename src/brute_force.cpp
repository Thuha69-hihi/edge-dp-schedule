// Vét cạn: duyệt mọi tập điểm cắt (subset mask trên {1..N-1}) có tối đa (M-1) phần tử.
// Độ phức tạp O(2^(N-1) * N), chỉ dùng cho N nhỏ (<= ~20) làm chuẩn đối chiếu.
//
// Quy ước: cost(k, i) của CostModel tính đoạn [k+1..i] (ký hiệu tài liệu Cost(k+1, i)).
//   mask bit (i-1) = 1 nghĩa là cắt ngay sau τ_i.
//   Bài toán dùng tối đa M đoạn = (M-1) điểm cắt trong. Ràng buộc về số node là popcount(mask)+1 <= M.
//   Vô nghiệm khi mọi mask đều có cost(0, N) == +inf hoặc popcount > M-1.
//
// Đầu ra: solution có cuts là chỉ số 1-index của các điểm cắt (loại bỏ cắt sau τ_N vì đó là
// đoạn cuối, luôn tồn tại); segments khớp với cuts và CostModel.cost.
#include <cmath>
#include <limits>
#include <vector>

#include "edgedp/cost.hpp"
#include "edgedp/solver.hpp"

namespace edgedp {

// Ghi chú: de chi phi 2^(N-1) nen chi dung khi N nho (<= ~16). Test cross_check voi seed lon
// (N den 40) se goi NotImplemented va duoc test_main bo qua (SKIP), de tranh treo may.
Solution solve_bruteforce(const Instance& in) {
    if (in.n == 0) {
        Solution s;
        s.feasible = true;
        s.energy = 0;
        s.nodes_used = 0;
        return s;
    }
    if (in.m == 0) return Solution{};  // có việc mà không có node -> vô nghiệm
    if (in.n > 18) {
        // Khong nem loi: N qua lon, tra NotImplemented de test runner bo qua.
        throw NotImplemented("solve_bruteforce: N > 18, chi dung khi kiem tra nho");
    }

    const CostModel cm(in);
    const ld INF = inf();

    Solution best;
    best.feasible = false;
    best.energy = INF;
    ld best_total = INF;
    std::vector<int> best_cuts;

    // mask bit i-1 (0 <= i-1 <= n-2) = cắt ngay sau τ_i. Có tối đa M-1 bit bật.
    const unsigned total = 1u << (in.n > 0 ? (in.n - 1) : 0);
    for (unsigned mask = 0; mask < total; ++mask) {
        if (static_cast<int>(__builtin_popcount(mask)) >= in.m) continue;  // > M-1 cắt
        bool ok = true;
        ld total_cost = 0;
        int prev = 0;
        std::vector<int> cuts;  // chỉ số cắt 1-index
        for (int i = 1; i <= in.n; ++i) {
            const bool cut_after = (i == in.n) || ((mask >> (i - 1)) & 1u);
            if (!cut_after) continue;
            const ld c = cm.cost(prev, i);
            if (c == INF) { ok = false; break; }
            total_cost += c;
            if (i != in.n) cuts.push_back(i);
            prev = i;
        }
        if (!ok) continue;
        if (total_cost < best_total) {
            best_total = total_cost;
            best_cuts = cuts;
        }
    }

    if (best_total == INF) return Solution{};

    // Truy vết segments từ cuts + best_total.
    Solution out;
    out.feasible = true;
    out.energy = best_total;
    out.nodes_used = static_cast<int>(best_cuts.size()) + 1;
    out.cuts = best_cuts;
    int prev = 0;
    for (size_t s = 0; s <= best_cuts.size(); ++s) {
        const int to = (s < best_cuts.size()) ? best_cuts[s] : in.n;
        out.segments.push_back({prev + 1, to, cm.work(prev, to), cm.cost(prev, to)});
        prev = to;
    }
    return out;
}

}  // namespace edgedp
