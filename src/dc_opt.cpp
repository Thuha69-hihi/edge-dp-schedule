#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "edgedp/cost.hpp"
#include "edgedp/solver.hpp"

using namespace std;

namespace edgedp {

namespace {

struct Layer {
    const CostModel& cm;
    const vector<ld>& prev;
    vector<ld>& cur;
    vector<int>& cur_opt;
    const vector<int>& P;      // ứng viên k, tăng dần
    const vector<int>& rows;   // các hàng i khả thi, tăng dần
    const vector<int>& pl;     // pl[r], pr[r]: cửa sổ chỉ số P của hàng rows[r]
    const vector<int>& pr;

    void compute(int rlo, int rhi, int optL, int optR) const {
        if (rlo > rhi) return;
        const int r = (rlo + rhi) / 2;
        const int i = rows[r];
        const int a = max(optL, pl[r]);
        const int b = min(optR, pr[r]);

        ld best = inf();
        int arg = -1;
        for (int t = a; t <= b; ++t) {
            const int k = P[t];
            const ld c = cm.cost(k, i);
            if (c == inf()) continue;  
            const ld v = prev[k] + c;
            if (v < best) { best = v; arg = t; }  
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

} 

Solution solve_dc(const Instance& in) {
    const int n = in.n;
    const int m = in.m;
    Solution sol;
    if (n == 0) { sol.feasible = true; return sol; }
    if (m == 0) return sol;

    const CostModel cm(in);
    const ld INF = inf();
    const ld cap = in.params.cap;


    for (int t = 1; t <= n; ++t) {
        if (static_cast<ld>(cm.work(t - 1, t)) > cap) return sol;
    }

    vector<int> lo(n + 1, 0);
    {
        int p = 0;
        for (int i = 1; i <= n; ++i) {
            while (static_cast<ld>(cm.work(p, i)) > cap) ++p;
            lo[i] = p;
        }
    }
    // valid_start[k]: đoạn có thể bắt đầu sau điểm cắt k (k = 0 không chịu link_cap).
    vector<char> valid_start(n + 1, 1);
    for (int k = 1; k <= n; ++k) {
        valid_start[k] = static_cast<ld>(in.d[k]) <= in.params.link_cap;
    }

    vector<ld> prev(n + 1, INF), cur(n + 1, INF);
    prev[0] = 0;
    vector<vector<int>> opt(m + 1, vector<int>(n + 1, -1));
    vector<ld> energy_per_j(m + 1, INF);

    vector<int> P, rows, pl, pr;
    P.reserve(n + 1); rows.reserve(n); pl.reserve(n); pr.reserve(n);

    for (int j = 1; j <= m; ++j) {
        fill(cur.begin(), cur.end(), INF);

        P.clear();
        for (int k = 0; k <= n - 1; ++k) {
            if (prev[k] != INF && valid_start[k]) P.push_back(k);
        }

        // Cửa sổ chỉ số P của từng hàng; hai con trỏ vì lo(i) và i-1 đều không giảm.
        rows.clear(); pl.clear(); pr.clear();
        const int sz = static_cast<int>(P.size());
        int pp = 0, qq = 0;
        for (int i = 1; i <= n; ++i) {
            while (pp < sz && P[pp] < lo[i]) ++pp;
            while (qq < sz && P[qq] <= i - 1) ++qq;
            const int l = pp, r = qq - 1;
            if (l <= r) {
                rows.push_back(i);
                pl.push_back(l);
                pr.push_back(r);
            }
        }

        if (!rows.empty()) {
            const Layer layer{cm, prev, cur, opt[j], P, rows, pl, pr};
            layer.compute(0, static_cast<int>(rows.size()) - 1, 0, sz - 1);
        }

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

    vector<int> ends;
    int i = n;
    for (int j = jbest; j >= 1; --j) {
        const int k = opt[j][i];
        ends.push_back(i);
        i = k;
    }
    ends.push_back(0);
    reverse(ends.begin(), ends.end());
    for (int s = 1; s <= jbest; ++s) {
        const int k = ends[s - 1], e = ends[s];
        sol.segments.push_back({k + 1, e, cm.work(k, e), cm.cost(k, e)});
        if (s < jbest) sol.cuts.push_back(e);
    }
    return sol;
}

}