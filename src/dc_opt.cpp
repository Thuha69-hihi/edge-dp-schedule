// Divide and Conquer Optimization cho DP(i, j) của baseline.
// Áp dụng được khi Cost(k, i) (với cost được định nghĩa trong cost.hpp) thỏa bất đẳng thức
// tứ giác (Quadrangle Inequality / Monge), nghĩa là opt(i, j) đơn điệu không giảm theo i:
//   opt(i, j) <= opt(i+1, j)
// Mục 5 tài liệu mô hình 1.05 chứng minh với mô hình bậc ba (κ/T²)·(S_i − S_k)^3 (g(x) lồi)
// và h(k) = ε·d_k chỉ phụ thuộc k, ta có QI nên D&C áp dụng được trực tiếp.
//
// Độ phức tạp: O(M · N log N) so với O(M · N^2) của baseline.
//
// Triển khai:
//   compute(j, lo, hi, optL, optR, prev_dp, cur_dp, prev_opt, cur_opt)
//     Với hàng j, tìm DP(i, j) cho i ∈ [lo, hi] với argmin k ∈ [max(j-1, optL), min(i-1, optR)].
//     mid = (lo+hi)//2, duyệt k từ optL đến min(mid-1, optR) chọn k tốt nhất -> best, gán
//     cur[mid] = best + Cost(best+1, mid) và cur_opt[mid] = best. Chia đôi:
//       compute(j, lo, mid-1, optL, best)
//       compute(j, mid+1, hi, best, optR)
//
// Lưu ý:
//   - Cost và prev[k] có thể +inf (vượt cap, d_k > link_cap, hoặc chưa đủ node).
//     Khi mọi k trong khoảng đều +inf thì cur[i] = +inf và cur_opt[i] = -1. Hàng đó
//     KHÔNG cung cấp ràng buộc đơn điệu: cả hai nhánh đệ quy giữ nguyên [optL, optR].
//   - Bộ nhớ: cur_dp và cur_opt có size (n+1).
#include <algorithm>
#include <limits>
#include <vector>

#include "edgedp/cost.hpp"
#include "edgedp/solver.hpp"

namespace edgedp {

namespace {

// Duyệt k ∈ [klo, khi] tìm argmin prev[k] + cost(k, i). Trả (best_value, best_k).
// Trả về (+inf, -1) nếu không có k khả thi.
std::pair<ld, int> best_k(const std::vector<ld>& prev,
                          const CostModel& cm,
                          int i,
                          int klo,
                          int khi) {
    ld best = inf();
    int arg = -1;
    for (int k = klo; k <= khi; ++k) {
        if (prev[k] == inf()) continue;
        const ld c = cm.cost(k, i);
        if (c == inf()) continue;
        const ld v = prev[k] + c;
        if (v < best) { best = v; arg = k; }
    }
    return {best, arg};
}

void compute(int j, int lo, int hi,
             int optL, int optR,
             const std::vector<ld>& prev,
             std::vector<ld>& cur,
             std::vector<int>& cur_opt,
             const CostModel& cm) {
    if (lo > hi) return;
    const int mid = (lo + hi) / 2;
    int klo = std::max(j - 1, optL);
    int khi = std::min(mid - 1, optR);
    auto [best_val, best_k_pos] = best_k(prev, cm, mid, klo, khi);
    cur[mid] = best_val;
    cur_opt[mid] = best_k_pos;

    // mid có nghiệm: chia khoảng theo best_k_pos (opt đơn điệu).
    // mid vô nghiệm: không có thông tin đơn điệu, giữ nguyên [optL, optR] cho cả hai nhánh.
    const int left_hi  = (best_k_pos >= 0) ? best_k_pos : optR;
    const int right_lo = (best_k_pos >= 0) ? best_k_pos : optL;

    compute(j, lo, mid - 1, optL, left_hi, prev, cur, cur_opt, cm);
    compute(j, mid + 1, hi, right_lo, optR, prev, cur, cur_opt, cm);
}

}  // namespace

Solution solve_dc(const Instance& in) {
    const int n = in.n;
    const int m = in.m;
    Solution sol;
    if (n == 0) { sol.feasible = true; return sol; }
    if (m == 0) return sol;

    const CostModel cm(in);
    const ld INF = inf();

    // prev (j=0) và "opt"(j=0) không tồn tại; quy ước prev[0]=0, các vị trí khác +inf.
    std::vector<ld> prev(n + 1, INF), cur(n + 1, INF);
    prev[0] = 0;
    std::vector<std::vector<int>> opt(m + 1, std::vector<int>(n + 1, -1));
    std::vector<ld> energy_per_j(m + 1, INF);

    for (int j = 1; j <= m; ++j) {
        std::fill(cur.begin(), cur.end(), INF);
        std::fill(opt[j].begin(), opt[j].end(), -1);

        compute(j, j, n, j - 1, n - 1, prev, cur, opt[j], cm);

        energy_per_j[j] = cur[n];
        prev.swap(cur);
    }

    int jbest = -1;
    ld ebest = INF;
    for (int j = 1; j <= m; ++j) {
        if (energy_per_j[j] < ebest) { ebest = energy_per_j[j]; jbest = j; }
    }
    if (jbest < 0) return sol;

    sol.feasible = true;
    sol.energy = ebest;
    sol.nodes_used = jbest;

    // Truy vết (cùng cách với baseline).
    std::vector<int> ends;
    int i = n;
    for (int j = jbest; j >= 1; --j) {
        const int k = opt[j][i];
        ends.push_back(i);
        i = k;
    }
    ends.push_back(0);
    std::reverse(ends.begin(), ends.end());
    for (int s = 1; s <= jbest; ++s) {
        const int k = ends[s - 1], e = ends[s];
        sol.segments.push_back({k + 1, e, cm.work(k, e), cm.cost(k, e)});
        if (s < jbest) sol.cuts.push_back(e);
    }
    return sol;
}

}  // namespace edgedp