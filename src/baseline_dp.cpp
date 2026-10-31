// Baseline DP, thời gian O(M * N^2), theo mục 4 của tài liệu mô hình:
//   DP(i, j) = min_{j-1 <= k < i} { DP(k, j-1) + Cost(k+1, i) },   DP(0,0) = 0, DP(i,0) = +inf (i > 0)
//   E* = min_{1 <= j <= M} DP(N, j)
// Bộ nhớ: giá trị DP chỉ giữ hai hàng (j-1 và j) -> O(N); bảng opt đầy đủ O(M*N) để truy vết.
// Khi hòa: chọn k nhỏ nhất (opt đơn điệu) và j nhỏ nhất.
#include <algorithm>

#include "edgedp/cost.hpp"
#include "edgedp/solver.hpp"

namespace edgedp {

Solution solve_baseline(const Instance& in) {
    const int n = in.n;
    const int m = in.m;
    Solution sol;
    if (n == 0) { sol.feasible = true; return sol; }  // không có việc: năng lượng 0, không bật node nào
    if (m == 0) return sol;                           // có việc mà không có node

    const CostModel cm(in);
    const ld INF = inf();

    // lo[i] = k nhỏ nhất có S_i - S_k <= cap (không giảm theo i). Mọi k < lo[i] có Cost = +inf nên bỏ qua hẳn
    // thay vì gọi cost() rồi loại: vòng trong chỉ chạy trên cửa sổ [lo[i], i-1]. Kết quả không đổi.
    std::vector<int> lo(n + 1, 0);
    for (int i = 1, p = 0; i <= n; ++i) {
        while (p < i && static_cast<ld>(cm.work(p, i)) > in.params.cap) ++p;
        lo[i] = p;
    }

    std::vector<ld> prev(n + 1, INF), cur(n + 1, INF);
    prev[0] = 0;                                       // hàng j = 0
    // Bảng opt phẳng (một khối liên tục) thay cho vector<vector<int>>: bớt m+1 lần cấp phát, truy cập liền kề.
    std::vector<int> opt(static_cast<size_t>(m + 1) * (n + 1), -1);
    const auto OPT = [&](int j, int i) -> int& { return opt[static_cast<size_t>(j) * (n + 1) + i]; };
    std::vector<ld> best_at_n(m + 1, INF);             // DP(N, j)

    for (int j = 1; j <= m; ++j) {
        std::fill(cur.begin(), cur.end(), INF);
        for (int i = j; i <= n; ++i) {
            ld best = INF;
            int arg = -1;
            for (int k = std::max(j - 1, lo[i]); k < i; ++k) {
                if (prev[k] == INF) continue;
                const ld c = cm.cost(k, i);
                if (c == INF) continue;
                const ld v = prev[k] + c;
                if (v < best) { best = v; arg = k; }
            }
            cur[i] = best;
            OPT(j, i) = arg;
        }
        best_at_n[j] = cur[n];
        prev.swap(cur);
    }

    int jbest = -1;
    ld ebest = INF;
    for (int j = 1; j <= m; ++j)
        if (best_at_n[j] < ebest) { ebest = best_at_n[j]; jbest = j; }
    if (jbest < 0) return sol;  // không khả thi: feasible = false

    sol.feasible = true;
    sol.energy = ebest;
    sol.nodes_used = jbest;

    // Truy vết từ (N, jbest): opt[j][i] = k là điểm cắt trước đoạn cuối.
    std::vector<int> ends;  // c_j, c_{j-1}, ..., c_0 = 0 (ngược)
    int i = n;
    for (int j = jbest; j >= 1; --j) {
        const int k = OPT(j, i);
        ends.push_back(i);
        i = k;
    }
    ends.push_back(0);
    std::reverse(ends.begin(), ends.end());  // c_0 = 0, c_1, ..., c_j = N
    for (int s = 1; s <= jbest; ++s) {
        const int k = ends[s - 1], e = ends[s];
        sol.segments.push_back({k + 1, e, cm.work(k, e), cm.cost(k, e)});
        if (s < jbest) sol.cuts.push_back(e);
    }
    return sol;
}

}  // namespace edgedp
