// Hàm chi phí một đoạn, dùng chung cho mọi solver để cả nhóm tính đúng một công thức.
//
//   Cost(k+1, i) = E_on + (κ/T^2) * (S_i - S_k)^3 + ε * d_k
//   = +∞ nếu S_i - S_k > cap, hoặc (k >= 1 và d_k > link_cap)
//
// (k = 0: d_0 là dữ liệu từ thiết bị IoT, không chịu ràng buộc link_cap.)
#pragma once
#include <limits>

#include "types.hpp"

namespace edgedp {

inline ld inf() { return std::numeric_limits<ld>::infinity(); }

struct CostModel {
    const Instance& in;
    std::vector<ll> S;  // size n+1

    explicit CostModel(const Instance& instance) : in(instance), S(instance.n + 1, 0) {
        for (int i = 1; i <= in.n; ++i) S[i] = S[i - 1] + in.w[i - 1];
    }

    // Khối lượng đoạn (k, i], tức τ_{k+1}..τ_i.
    ll work(int k, int i) const { return S[i] - S[k]; }

    // C(k, i) với 0 <= k < i <= n.
    ld cost(int k, int i) const {
        const ll W = work(k, i);
        if (static_cast<ld>(W) > in.params.cap) return inf();
        if (k >= 1 && static_cast<ld>(in.d[k]) > in.params.link_cap) return inf();
        const ld x = static_cast<ld>(W);
        return in.params.e_on + in.params.kappa_over_T2 * x * x * x + in.params.eps * static_cast<ld>(in.d[k]);
    }
};

}  // namespace edgedp
