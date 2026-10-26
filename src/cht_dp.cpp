// Convex Hull Trick -> O(M*N) nếu hệ số góc và điểm truy vấn đơn điệu.
//
// LƯU Ý QUAN TRỌNG (mục 5 tài liệu mô hình): với mô hình bậc ba (S_i - S_k)^3, khai triển có các số hạng
// S_i^2*S_k và S_i*S_k^2, nên CHT chuẩn KHÔNG dùng được. CHT chỉ dùng cho biến thể bậc hai
//   E_exec(W) = a*W^2 + b*W :
//   DP(i,j) = a*S_i^2 + b*S_i + E_on + min_k { (-2a*S_k) * S_i + [DP(k,j-1) + a*S_k^2 - b*S_k + eps*d_k] }
// Hệ số góc -2a*S_k giảm dần theo k, điểm truy vấn S_i tăng dần -> deque, O(1) khấu hao.
//

#include "edgedp/solver.hpp"
#include "edgedp/types.hpp"
#include "edgedp/cost.hpp"
#include <vector>
#include <deque>
#include <algorithm>
#include <cmath>

namespace edgedp {

struct Line {
    ld m, c;
    int k;

    ld eval(ld x) const {
        return m * x + c;
    }

    ld intersect(const Line& other) const {
        if (std::fabs(m - other.m) < 1e-12) return 1e18;
        return (other.c - c) / (m - other.m);
    }
};

Solution solve_cht(const Instance& in) {
    if (in.n <= 0 || in.m <= 0) {
        return {in.n == 0, 0.0, 0, {}, {}, "cht", 0.0};
    }

    CostModel cm(in);
    int n = in.n;
    int M = in.m;

    std::vector<std::vector<ld>> dp(M + 1, std::vector<ld>(n + 1, inf()));
    std::vector<std::vector<int>> parent(M + 1, std::vector<int>(n + 1, -1));

    dp[0][0] = 0.0;
    ld a = in.params.kappa_over_T2;

    for (int j = 1; j <= M; ++j) {
        std::deque<Line> cht;

        for (int i = 1; i <= n; ++i) {
            int prev_k = i - 1;
            if (dp[j - 1][prev_k] < inf()) {
                bool valid_link = (prev_k == 0) || (static_cast<ld>(in.d[prev_k]) <= in.params.link_cap);
                if (valid_link) {
                    ld S_k = static_cast<ld>(cm.S[prev_k]);
                    ld m_val = -2.0 * a * S_k;
                    ld c_val = dp[j - 1][prev_k] + a * S_k * S_k + in.params.e_on;
                    if (prev_k >= 1) {
                        c_val += in.params.eps * static_cast<ld>(in.d[prev_k]);
                    }

                    Line new_line = {m_val, c_val, prev_k};

                    while (cht.size() >= 2) {
                        const Line& l1 = cht[cht.size() - 2];
                        const Line& l2 = cht.back();
                        if (l1.intersect(l2) >= l2.intersect(new_line)) {
                            cht.pop_back();
                        } else {
                            break;
                        }
                    }
                    cht.push_back(new_line);
                }
            }

            ld S_i = static_cast<ld>(cm.S[i]);

            while (cht.size() >= 2 && cht[0].eval(S_i) >= cht[1].eval(S_i)) {
                cht.pop_front();
            }

            if (!cht.empty()) {
                // Kiểm tra trực tiếp qua CostModel để đảm bảo khớp 100% chi phí chuẩn của baseline
                int best_k = cht.front().k;
                ll work_val = cm.work(best_k, i);

                if (static_cast<ld>(work_val) <= in.params.cap) {
                    ld exact_cost = cm.cost(best_k, i);
                    if (exact_cost < inf()) {
                        ld total_cost = dp[j - 1][best_k] + exact_cost;
                        if (total_cost < dp[j][i]) {
                            dp[j][i] = total_cost;
                            parent[j][i] = best_k;
                        }
                    }
                }
            }
        }
    }

    int best_j = -1;
    ld min_total_cost = inf();
    for (int j = 1; j <= M; ++j) {
        if (dp[j][n] < min_total_cost) {
            min_total_cost = dp[j][n];
            best_j = j;
        }
    }

    if (min_total_cost == inf() || best_j == -1) {
        return {false, 0.0, 0, {}, {}, "cht", 0.0};
    }

    std::vector<int> cuts;
    std::vector<Segment> segments;
    int curr = n;
    int curr_j = best_j;

    while (curr > 0 && curr_j > 0) {
        int p = parent[curr_j][curr];
        if (p == -1) break;
        if (curr < n) {
            cuts.push_back(curr);
        }
        curr = p;
        curr_j--;
    }
    std::reverse(cuts.begin(), cuts.end());

    int prev_pos = 0;
    for (int cut_pt : cuts) {
        segments.push_back({prev_pos + 1, cut_pt, cm.work(prev_pos, cut_pt), cm.cost(prev_pos, cut_pt)});
        prev_pos = cut_pt;
    }
    segments.push_back({prev_pos + 1, n, cm.work(prev_pos, n), cm.cost(prev_pos, n)});

    Solution sol;
    sol.feasible = true;
    sol.energy = min_total_cost;
    sol.nodes_used = best_j;
    sol.cuts = cuts;
    sol.segments = segments;
    sol.solver = "cht";
    sol.time_ms = 0.0;

    return sol;
}

} // namespace edgedp