
// Convex Hull Trick cho biến thể năng lượng BẬC HAI (model = Quadratic):
//   E_exec(W) = a*W^2 + b*W,  a >= 0   (tài liệu mô hình mục 5; quyết định họp tuần 1)
//
// Với S_i - S_k = W, khai triển ra:
//   DP(i,j) = E_on + a*S_i^2 + b*S_i + min_k { m_k * S_i + c_k }
//     m_k = -2a * S_k                                   (hệ số góc, KHÔNG TĂNG theo k)
//     c_k = DP(k,j-1) + a*S_k^2 - b*S_k + eps*d_k       (tung độ gốc, tính cả k = 0 vì d_0 cũng tốn eps)
// Mỗi k hợp lệ là một đường thẳng y = m_k*x + c_k, truy vấn tại x = S_i (không giảm theo i).
//
// Mô hình bậc BA (S_i - S_k)^3 có số hạng trộn S_i^2*S_k và S_i*S_k^2, không đưa được về đường thẳng nên CHT
// không áp dụng được: solve_cht ném NotImplemented (cubic dùng baseline hoặc D&C).
//
// Hai ràng buộc làm tập k hợp lệ của hàng i bị thu hẹp:
//   * link_cap: k >= 1 mà d_k > link_cap thì k không bao giờ được cắt -> không đưa đường k vào.
//   * cap: chỉ được dùng k trong cửa sổ [lo(i), i-1], lo(i) = k nhỏ nhất có S_i - S_k <= cap. lo(i) không
//   giảm.
//
// Có hai cách chạy, chọn theo cap:
//   1) Cửa sổ không bao giờ cắt (S_N <= cap): deque đơn điệu, O(1) khấu hao -> O(M*N).
//   2) Có cửa sổ: deque KHÔNG dùng được (đường bị loại khỏi bao lồi vì một đường mới hơn có thể cần lại khi
//      đường cũ hơn hết hạn). Dùng cây phân đoạn, mỗi nút giữ bao lồi của các đường trong khoảng của nó; truy
//      vấn [lo(i), i-1] ghép từ O(log N) nút, mỗi nút có con trỏ tiến dần theo x tăng -> O(M*N*log N).
// Cả hai cách đều chọn k bằng bao lồi (số thực), rồi TÍNH LẠI chi phí chính xác bằng CostModel::cost để DP
// khớp từng bit với baseline khi chọn trùng k.
#include <algorithm>
#include <stdexcept>
#include <vector>

#include "edgedp/cost.hpp"
#include "edgedp/solver.hpp"

namespace edgedp {

namespace {

// Các đường thẳng của một tầng j, đánh chỉ số theo k = 0..n-1.
struct Lines {
    std::vector<ld> m, c;
    std::vector<char> ok;  // đường k có được dùng không (DP(k,j-1) hữu hạn và không vi phạm link_cap)
};

inline ld at(const Lines& L, int k, ld x) {
    return L.m[k] * x + L.c[k];
}

// Đường b nằm giữa a và nw (hệ số góc giảm dần: m_a > m_b > m_nw) có vô dụng không?
// b vô dụng nếu nw vượt a không muộn hơn b vượt a:  x(a,nw) <= x(a,b). Nhân chéo để khỏi chia.
inline bool useless(const Lines& L, int a, int b, int nw) {
    return (L.c[nw] - L.c[a]) * (L.m[a] - L.m[b]) <= (L.c[b] - L.c[a]) * (L.m[a] - L.m[nw]);
}

// Thêm đường nw vào bao lồi dưới st[head..cnt). Yêu cầu hệ số góc của nw <= mọi đường đang có
// (đường được thêm theo k tăng dần). Khi cùng hệ số góc giữ đường có c nhỏ hơn (hòa thì giữ k nhỏ hơn).
inline void hull_push(const Lines& L, int* st, int& cnt, int head, int nw) {
    if (cnt > head) {
        const int b = st[cnt - 1];
        if (L.m[b] == L.m[nw]) {
            if (!(L.c[nw] < L.c[b])) return;
            --cnt;
        }
    }
    while (cnt - head >= 2 && useless(L, st[cnt - 2], st[cnt - 1], nw))
        --cnt;
    st[cnt++] = nw;
}

// ---- Cách 1: deque, dùng khi cap không bao giờ cắt cửa sổ ----
void layer_deque(const Lines& L, const CostModel& cm, int n, const std::vector<ld>& prev,
                 std::vector<ld>& cur, int* opt) {
    std::vector<int> st(n + 1);
    int head = 0, cnt = 0;
    for (int i = 1; i <= n; ++i) {
        const int knew = i - 1;  // đường k = i-1 trở nên dùng được từ hàng i
        if (L.ok[knew]) hull_push(L, st.data(), cnt, head, knew);
        if (cnt == head) continue;
        const ld x = static_cast<ld>(cm.S[i]);
        while (cnt - head >= 2 && at(L, st[head + 1], x) < at(L, st[head], x))
            ++head;
        const int k = st[head];
        const ld cst = cm.cost(k, i);
        if (cst == inf()) continue;
        cur[i] = prev[k] + cst;
        opt[i] = k;
    }
}

// ---- Cách 2: cây phân đoạn các bao lồi, dùng khi cap tạo cửa sổ trượt ----
struct HullTree {
    int n = 0, size = 1, levels = 0;
    std::vector<int> pool;   // nút ở tầng l chiếm dải [l*size, (l+1)*size) của pool
    std::vector<int> start;  // vị trí bắt đầu bao lồi của từng nút trong pool
    std::vector<int> cnt;    // số đường trong bao lồi của nút
    std::vector<int> ptr;    // con trỏ truy vấn của nút (tiến dần vì x không giảm)

    explicit HullTree(int n_) : n(n_) {
        while (size < n) {
            size <<= 1;
            ++levels;
        }
        pool.assign(static_cast<size_t>(levels + 1) * size, 0);
        start.assign(2 * static_cast<size_t>(size), 0);
        cnt.assign(2 * static_cast<size_t>(size), 0);
        ptr.assign(2 * static_cast<size_t>(size), 0);
        for (int l = 0; l <= levels; ++l) {
            const int base = size >> l;
            for (int off = 0; off < base; ++off)
                start[base + off] =
                    static_cast<int>(static_cast<size_t>(l) * size + static_cast<size_t>(off) * (1u << l));
        }
    }

    void build(const Lines& L) {
        for (int k = 0; k < size; ++k) {
            const int v = size + k;
            ptr[v] = 0;
            cnt[v] = 0;
            if (k < n && L.ok[k]) {
                pool[start[v]] = k;
                cnt[v] = 1;
            }
        }
        for (int v = size - 1; v >= 1; --v) {
            ptr[v] = 0;
            int c = 0;
            int* st = &pool[start[v]];
            for (int u = 2 * v; u <= 2 * v + 1; ++u)
                for (int t = 0; t < cnt[u]; ++t)
                    hull_push(L, st, c, 0, pool[start[u] + t]);
            cnt[v] = c;
        }
    }

    // k cho giá trị nhỏ nhất tại x trong [l, r]; -1 nếu không có đường nào.
    int query(const Lines& L, int l, int r, ld x) {
        int best = -1;
        ld bestv = inf();
        auto visit = [&](int v) {
            const int c = cnt[v];
            if (c == 0) return;
            const int* h = &pool[start[v]];
            int& p = ptr[v];
            ld curv = at(L, h[p], x);
            while (p + 1 < c) {
                const ld nx = at(L, h[p + 1], x);
                if (nx < curv) {
                    curv = nx;
                    ++p;
                } else
                    break;
            }
            if (best < 0 || curv < bestv || (curv == bestv && h[p] < best)) {
                bestv = curv;
                best = h[p];
            }
        };
        int lo = l + size, hi = r + size + 1;
        while (lo < hi) {
            if (lo & 1) visit(lo++);
            if (hi & 1) visit(--hi);
            lo >>= 1;
            hi >>= 1;
        }
        return best;
    }
};

void layer_tree(HullTree& tree, const Lines& L, const CostModel& cm, int n, ld cap,
                const std::vector<ld>& prev, std::vector<ld>& cur, int* opt) {
    tree.build(L);
    int lo = 0;
    for (int i = 1; i <= n; ++i) {
        while (lo<i&& static_cast<ld>(cm.S[i] - cm.S[lo])> cap)
            ++lo;
        if (lo > i - 1) continue;  // tác vụ τ_i một mình đã vượt cap
        const int k = tree.query(L, lo, i - 1, static_cast<ld>(cm.S[i]));
        if (k < 0) continue;
        const ld cst = cm.cost(k, i);
        if (cst == inf()) continue;
        cur[i] = prev[k] + cst;
        opt[i] = k;
    }
}

}  // namespace

Solution solve_cht(const Instance& in) {
    const Params& P = in.params;
    if (P.model != EnergyModel::Quadratic)
        throw NotImplemented("solve_cht: chi ho tro model quadratic (cubic dung baseline hoac dc)");
    if (P.quad_a < 0) throw std::invalid_argument("solve_cht: can quad_a >= 0");

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
    const ld a = P.quad_a, b = P.quad_b;
    const bool windowed = static_cast<ld>(cm.S[n]) > P.cap;  // có i nào bị cap cắt cửa sổ không

    Lines L;
    L.m.assign(n, 0);
    L.c.assign(n, 0);
    L.ok.assign(n, 0);
    HullTree tree(windowed ? n : 1);

    std::vector<ld> prev(n + 1, INF), cur(n + 1, INF);
    prev[0] = 0;
    // Bảng opt phẳng một khối liên tục (thay vector<vector<int>>).
    std::vector<int> opt(static_cast<size_t>(m + 1) * (n + 1), -1);
    const auto OPT = [&](int j, int i) -> int* { return opt.data() + static_cast<size_t>(j) * (n + 1) + i; };
    std::vector<ld> best_at_n(m + 1, INF);

    for (int j = 1; j <= m; ++j) {
        bool any = false;
        for (int k = 0; k < n; ++k) {
            const bool ok = prev[k] != INF && (k == 0 || static_cast<ld>(in.d[k]) <= P.link_cap);
            L.ok[k] = ok;
            if (!ok) continue;
            any = true;
            const ld Sk = static_cast<ld>(cm.S[k]);
            L.m[k] = -2 * a * Sk;
            L.c[k] = prev[k] + a * Sk * Sk - b * Sk + P.eps * static_cast<ld>(in.d[k]);
        }
        if (!any) break;  // không còn trạng thái hữu hạn: các tầng sau cũng vô nghiệm

        std::fill(cur.begin(), cur.end(), INF);
        if (windowed)
            layer_tree(tree, L, cm, n, P.cap, prev, cur, OPT(j, 0));
        else
            layer_deque(L, cm, n, prev, cur, OPT(j, 0));
        best_at_n[j] = cur[n];
        prev.swap(cur);
    }

    int jbest = -1;
    ld ebest = INF;
    for (int j = 1; j <= m; ++j)
        if (best_at_n[j] < ebest) {
            ebest = best_at_n[j];
            jbest = j;
        }
    if (jbest < 0) return sol;  // không khả thi

    sol.feasible = true;
    sol.energy = ebest;
    sol.nodes_used = jbest;

    // Truy vết giống baseline: opt[j][i] = k là điểm cắt trước đoạn cuối.
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
