// Quy hoạch động Chia để trị (Divide & Conquer Optimization), thời gian O(M * N * log N), bộ nhớ O(M * N).
//
// Công thức chuyển trạng thái ở mỗi tầng j = 1..M:
//   DP(i, j) = min_{k thuộc P, lo[i] <= k < i} { DP(k, j-1) + Cost(k+1, i) }
//
// Tính chất đơn điệu của điểm cắt tối ưu (chứng minh ở mục 5 tài liệu mô hình):
//   Hàm chi phí thực thi g(W) lồi chặt (cả bậc ba W^3 lẫn bậc hai a*W^2 + b*W với a >= 0) nên thỏa
//   bất đẳng thức tứ giác (Quadrangle Inequality - QI) trên miền hữu hạn:
//     Cost(k+1, i) + Cost(k'+1, i') <= Cost(k+1, i') + Cost(k'+1, i)   với mọi k < k' < i < i'.
//   Do đó opt(j, i) <= opt(j, i+1).
//
// Xử lý hai ràng buộc cứng để không phá tính đơn điệu vì các ô +inf:
//   * link_cap: lọc trước mảng ứng viên P chỉ giữ các k có DP(k, j-1) hữu hạn và d_k <= link_cap (k >= 1).
//   * cap: mỗi hàng i chỉ xét ứng viên P[t] thuộc cửa sổ [pl[r], pr[r]] tương ứng với k thuộc [lo[i], i-1].
//   Khi một hàng i không có ứng viên hợp lệ (arg < 0), giữ nguyên khoảng [optL, optR] cho hai nửa đệ quy.
//   Khi hòa: chọn k nhỏ nhất (phép so < chặt) để bảo toàn tính đơn điệu của opt.
#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "edgedp/cost.hpp"
#include "edgedp/solver.hpp"

namespace edgedp {

namespace {

struct Layer {
    const CostModel& cm;
    const std::vector<ld>& prev;
    std::vector<ld>& cur;
    int* cur_opt;
    const std::vector<int>& P;     // ứng viên k hợp lệ, tăng dần
    const std::vector<int>& rows;  // các hàng i có cửa sổ ứng viên khác rỗng, tăng dần
    const std::vector<int>& pl;    // pl[r], pr[r]: cửa sổ chỉ số trên P của hàng rows[r]
    const std::vector<int>& pr;

    void compute(int rlo, int rhi, int optL, int optR) const {
        if (rlo > rhi) return;
        const int r = (rlo + rhi) / 2;
        const int i = rows[r];
        const int a = std::max(optL, pl[r]);
        const int b = std::min(optR, pr[r]);

        ld best = inf();
        int arg = -1;
        for (int t = a; t <= b; ++t) {
            const int k = P[t];
            const ld c = cm.cost(k, i);
            if (c == inf()) continue;
            const ld v = prev[k] + c;
            if (v < best) {
                best = v;
                arg = t;
            }
        }
        if (arg < 0) {
            cur[i] = inf();
            cur_opt[i] = -1;
            compute(rlo, r - 1, optL, optR);
            compute(r + 1, rhi, optL, optR);
            return;
        }
        cur[i] = best;
        cur_opt[i] = P[arg];
        compute(rlo, r - 1, optL, arg);
        compute(r + 1, rhi, arg, optR);
    }
};

}  // namespace

Solution solve_dc(const Instance& in) {
    const int n = in.n;
    const int m = in.m;
    Solution sol;
    if (n == 0) {
        sol.feasible = true;
        return sol;
    }
    if (m == 0) return sol;

    const CostModel cm(in);
    const ld INF = inf();
    const ld cap = in.params.cap;

    for (int t = 1; t <= n; ++t) {
        if (static_cast<ld>(cm.work(t - 1, t)) > cap) return sol;
    }

    std::vector<int> lo(n + 1, 0);
    for (int i = 1, p = 0; i <= n; ++i) {
        while (static_cast<ld>(cm.work(p, i)) > cap)
            ++p;
        lo[i] = p;
    }

    // valid_start[k]: đoạn mới có thể bắt đầu ngay sau điểm cắt k (k = 0 không chịu link_cap).
    std::vector<char> valid_start(n + 1, 1);
    for (int k = 1; k <= n; ++k) {
        valid_start[k] = static_cast<ld>(in.d[k]) <= in.params.link_cap;
    }

    std::vector<ld> prev(n + 1, INF), cur(n + 1, INF);
    prev[0] = 0;
    // Bảng opt phẳng một khối liên tục (thay vector<vector<int>>): ít cấp phát, truy cập liền kề.
    std::vector<int> opt(static_cast<size_t>(m + 1) * (n + 1), -1);
    const auto OPT = [&](int j, int i) -> int* { return opt.data() + static_cast<size_t>(j) * (n + 1) + i; };
    std::vector<ld> best_at_n(m + 1, INF);

    std::vector<int> P, rows, pl, pr;
    P.reserve(n + 1);
    rows.reserve(n);
    pl.reserve(n);
    pr.reserve(n);

    for (int j = 1; j <= m; ++j) {
        std::fill(cur.begin(), cur.end(), INF);

        P.clear();
        for (int k = 0; k <= n - 1; ++k) {
            if (prev[k] != INF && valid_start[k]) P.push_back(k);
        }

        // Cửa sổ chỉ số P của từng hàng; hai con trỏ vì lo[i] và i-1 đều không giảm.
        rows.clear();
        pl.clear();
        pr.clear();
        const int sz = static_cast<int>(P.size());
        int pp = 0, qq = 0;
        for (int i = 1; i <= n; ++i) {
            while (pp < sz && P[pp] < lo[i])
                ++pp;
            while (qq < sz && P[qq] <= i - 1)
                ++qq;
            const int l = pp, r = qq - 1;
            if (l <= r) {
                rows.push_back(i);
                pl.push_back(l);
                pr.push_back(r);
            }
        }

        if (!rows.empty()) {
            const Layer layer{cm, prev, cur, OPT(j, 0), P, rows, pl, pr};
            layer.compute(0, static_cast<int>(rows.size()) - 1, 0, sz - 1);
        }

        best_at_n[j] = cur[n];
        prev.swap(cur);
    }

    int jbest = -1;
    ld ebest = INF;
    for (int j = 1; j <= m; ++j) {
        if (best_at_n[j] < ebest) {
            ebest = best_at_n[j];
            jbest = j;
        }
    }
    if (jbest < 0) return sol;

    sol.feasible = true;
    sol.energy = ebest;
    sol.nodes_used = jbest;

    // Truy vết từ (N, jbest): opt[j][i] = k là điểm cắt trước đoạn cuối.
    std::vector<int> ends;
    int i = n;
    for (int j = jbest; j >= 1; --j) {
        const int k = *OPT(j, i);
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
