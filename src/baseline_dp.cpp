// Baseline DP, độ phức tạp O(M * N^2).
//   dp[i] = min over j<i, node k of  dp[j] + alpha_k * (P[i]-P[j])^2 + setup_k
// với P là mảng tổng tiền tố của work. dp[0] = 0.
// TODO(TV3, tuần 2): đối chiếu với công thức TV1 chốt, thêm ràng buộc deadline.
#include <algorithm>
#include <limits>

#include "edgedp/solver.hpp"

namespace edgedp {

Solution solve_baseline(const Instance& in) {
    const int n = static_cast<int>(in.work.size());
    const int m = static_cast<int>(in.nodes.size());
    Solution sol;
    if (n == 0) { sol.feasible = true; return sol; }
    if (m == 0) return sol;  // có việc mà không có node -> vô nghiệm

    std::vector<ll> P(n + 1, 0);
    for (int i = 0; i < n; ++i) P[i + 1] = P[i] + in.work[i];

    const ll INF = std::numeric_limits<ll>::max() / 4;
    std::vector<ll> dp(n + 1, INF);
    std::vector<int> prevJ(n + 1, -1), prevK(n + 1, -1);
    dp[0] = 0;

    for (int i = 1; i <= n; ++i) {
        for (int j = 0; j < i; ++j) {
            const ll w = P[i] - P[j];
            for (int k = 0; k < m; ++k) {
                const ll c = dp[j] + in.nodes[k].alpha * w * w + in.nodes[k].setup;
                if (c < dp[i]) { dp[i] = c; prevJ[i] = j; prevK[i] = k; }
            }
        }
    }

    sol.feasible = true;
    sol.energy = dp[n];
    for (int i = n; i > 0; i = prevJ[i]) sol.batches.push_back({prevJ[i], i, prevK[i]});
    std::reverse(sol.batches.begin(), sol.batches.end());
    return sol;
}

}  // namespace edgedp
