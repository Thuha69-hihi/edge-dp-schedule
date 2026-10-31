// Vét cạn: duyệt mọi tập điểm cắt (subset mask trên {1..N-1}) có tối đa (M-1) phần tử.
// Độ phức tạp O(2^(N-1) * N), chỉ dùng cho N nhỏ (N <= 18) làm chuẩn đối chiếu.
//
// Quy ước: cost(k, i) của CostModel tính đoạn [k+1..i] (ký hiệu tài liệu Cost(k+1, i)).
//   mask bit (i-1) = 1 nghĩa là cắt ngay sau τ_i.
//   Bài toán dùng tối đa M đoạn = (M-1) điểm cắt trong. Ràng buộc về số node là popcount(mask)+1 <= M.
//   Vô nghiệm khi mọi mask đều có đoạn chi phí +inf hoặc popcount > M-1.
//
// Đầu ra: Solution có cuts là chỉ số 1-index của các điểm cắt (loại bỏ cắt sau τ_N vì đó là
// đoạn cuối, luôn tồn tại); segments khớp với cuts và CostModel::cost.
#include <cmath>
#include <limits>
#include <vector>

#include "edgedp/cost.hpp"
#include "edgedp/solver.hpp"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace edgedp {

namespace {

inline int popcount32(unsigned int x) {
#if defined(_MSC_VER)
    return static_cast<int>(__popcnt(x));
#else
    return __builtin_popcount(x);
#endif
}

}  // namespace

// Ghi chú: vì chi phí O(2^(N-1) * N) nên chỉ chạy khi N <= 18. Test cross_check với N lớn hơn
// sẽ nhận NotImplemented và được test_main bỏ qua (SKIP) để tránh treo máy.
Solution solve_bruteforce(const Instance& in) {
    Solution sol;
    if (in.n == 0) {
        sol.feasible = true;
        return sol;
    }
    if (in.m == 0) return sol;  // có việc mà không có node -> vô nghiệm
    if (in.n > 18) {
        // N quá lớn: ném NotImplemented để test runner tự động bỏ qua (SKIP).
        throw NotImplemented("solve_bruteforce: N > 18, chi dung khi kiem tra nho");
    }

    const CostModel cm(in);
    const ld INF = inf();

    ld best_total = INF;
    std::vector<int> best_cuts;

    // mask bit i-1 (0 <= i-1 <= n-2) = cắt ngay sau τ_i. Có tối đa M-1 bit bật.
    const unsigned total = 1u << (in.n > 0 ? (in.n - 1) : 0);
    for (unsigned mask = 0; mask < total; ++mask) {
        if (popcount32(mask) >= in.m) continue;  // > M-1 điểm cắt
        bool ok = true;
        ld total_cost = 0;
        int prev = 0;
        std::vector<int> cuts;  // chỉ số cắt 1-index
        for (int i = 1; i <= in.n; ++i) {
            const bool cut_after = (i == in.n) || ((mask >> (i - 1)) & 1u);
            if (!cut_after) continue;
            const ld c = cm.cost(prev, i);
            if (c == INF) {
                ok = false;
                break;
            }
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

    if (best_total == INF) return sol;

    // Truy vết segments từ best_cuts + best_total.
    sol.feasible = true;
    sol.energy = best_total;
    sol.nodes_used = static_cast<int>(best_cuts.size()) + 1;
    sol.cuts = best_cuts;
    int prev = 0;
    for (size_t s = 0; s <= best_cuts.size(); ++s) {
        const int to = (s < best_cuts.size()) ? best_cuts[s] : in.n;
        sol.segments.push_back({prev + 1, to, cm.work(prev, to), cm.cost(prev, to)});
        prev = to;
    }
    return sol;
}

}  // namespace edgedp
