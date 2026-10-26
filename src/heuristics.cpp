// 4 heuristic baseline (việc 2.08, TV5). Mô hình năng lượng đúng như tài liệu 1.05:
//   Cost(k+1, i) = E_on + E_exec(S_i - S_k) + eps * d_k
//   E_exec(W) = kappa_over_T2 * W^3            (chỉ mô hình bậc ba, khớp cost.hpp/types.hpp)
//   Vi phạm: S_i - S_k > cap, hoặc (k >= 1 và d_k > link_cap).
//
// Mọi phụ thuộc vào tên trường của Instance nằm trong struct View bên dưới
// (in.n, in.m, in.w[0..n-1] = w_1..w_n, in.d[0..n-1] = d_0..d_{n-1}, in.params.*).
#include "edgedp/heuristics.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

#include "edgedp/cost.hpp"

namespace edgedp {
namespace {

struct View {
    int n = 0, m = 0;
    std::vector<int64_t> S;  // S[0..n]
    std::vector<ld> d;       // d[0..n-1]
    ld kappa = 0, e_on = 0, eps = 0, cap = 0, link_cap = 0;

    explicit View(const Instance& in) : n(in.n), m(in.m) {
        S.assign(n + 1, 0);
        d.assign(n, 0);
        for (int i = 1; i <= n; ++i) S[i] = S[i - 1] + in.w[i - 1];
        for (int i = 0; i < n; ++i) d[i] = static_cast<ld>(in.d[i]);
        const auto& p = in.params;
        kappa = p.kappa_over_T2;
        e_on = p.e_on;
        eps = p.eps;
        cap = p.cap;
        link_cap = p.link_cap;
    }
    ld exec(ld W) const { return kappa * W * W * W; }
    bool cut_allowed(int c) const { return d[c] <= link_cap; }  // 1 <= c <= n-1
};

// Đánh giá một cách chia thành các đoạn liên tiếp; ends = c_1 < ... < c_j = n.
HeuristicResult evaluate_contiguous(const View& v, const char* name, const std::vector<int>& ends) {
    HeuristicResult r;
    r.name = name;
    int k = 0;
    for (int e : ends) {
        const ld W = static_cast<ld>(v.S[e] - v.S[k]);
        r.energy += v.e_on + v.exec(W) + v.eps * v.d[k];
        if (W > v.cap) ++r.violated_cap;
        if (k >= 1 && v.d[k] > v.link_cap) ++r.violated_link;
        if (e < v.n) r.cuts.push_back(e);
        k = e;
    }
    r.nodes_used = static_cast<int>(ends.size());
    r.feasible = r.violated_cap == 0 && r.violated_link == 0 && r.nodes_used <= v.m;
    return r;
}

HeuristicResult trivial(const View& v, const char* name) {
    HeuristicResult r;
    r.name = name;
    r.feasible = (v.n == 0);  // không có việc: năng lượng 0; có việc mà không có node: không khả thi
    return r;
}

}  // namespace

// Greedy first-fit: dồn tác vụ vào node hiện tại tới khi sắp vượt cap, rồi sang node kế.
// Điểm cắt luôn chọn ở vị trí cho phép (d_c <= link_cap) xa nhất có thể. Cách này cho số
// node ít nhất (tối ưu về số đoạn) nên tiết kiệm E_on, nhưng đẩy mọi node lên gần f_max
// nên E_exec lớn: baseline hợp lý cho một bộ lập lịch chỉ biết deadline, không biết DVFS.
// Nếu hết M node mà còn việc thì node cuối nhận hết phần còn lại (và bị tính vi phạm).
HeuristicResult solve_greedy(const Instance& in) {
    const View v(in);
    if (v.n == 0 || v.m <= 0) return trivial(v, "greedy");
    std::vector<int> ends;
    int k = 0;
    while (k < v.n) {
        if (static_cast<int>(ends.size()) == v.m - 1) {
            ends.push_back(v.n);
            break;
        }
        // i lớn nhất sao cho S[i] - S[k] <= cap
        int imax = static_cast<int>(std::upper_bound(v.S.begin() + k + 1, v.S.end(),
                                                      v.S[k] + static_cast<int64_t>(v.cap)) -
                                    v.S.begin()) - 1;
        if (imax <= k) imax = k + 1;  // một tác vụ đơn lẻ đã vượt cap: buộc phải nhận (vi phạm)
        int i = imax;
        while (i > k && i < v.n && !v.cut_allowed(i)) --i;
        if (i == k) i = imax;  // không có điểm cắt hợp lệ trong tầm với: chấp nhận vi phạm link
        ends.push_back(i);
        k = i;
    }
    return evaluate_contiguous(v, "greedy", ends);
}

// Chia đều số tác vụ: j = min(M, N) đoạn, đoạn s kết thúc tại floor(s*N/j).
HeuristicResult solve_equal_split(const Instance& in) {
    const View v(in);
    if (v.n == 0 || v.m <= 0) return trivial(v, "equal");
    const int j = std::min(v.m, v.n);
    std::vector<int> ends;
    for (int s = 1; s <= j; ++s)
        ends.push_back(static_cast<int>(static_cast<long long>(s) * v.n / j));
    return evaluate_contiguous(v, "equal", ends);
}

// Toàn bộ chuỗi chạy trên một node.
HeuristicResult solve_single_node(const Instance& in) {
    const View v(in);
    if (v.n == 0 || v.m <= 0) return trivial(v, "single");
    return evaluate_contiguous(v, "single", {v.n});
}

// Round-robin: tác vụ i (1-based) chạy trên node (i-1) mod M. Các tác vụ của một node không
// liên tiếp nên mọi cặp tác vụ kề nhau nằm trên hai node khác nhau (nếu M >= 2): phải truyền
// d_1..d_{N-1} qua liên kết, cộng d_0 lúc tải lên. Node s chạy tổng khối lượng của riêng nó.
HeuristicResult solve_round_robin(const Instance& in) {
    const View v(in);
    if (v.n == 0 || v.m <= 0) return trivial(v, "roundrobin");
    std::vector<int64_t> load(v.m, 0);
    for (int i = 1; i <= v.n; ++i) load[(i - 1) % v.m] += v.S[i] - v.S[i - 1];
    HeuristicResult r;
    r.name = "roundrobin";
    for (int s = 0; s < v.m; ++s) {
        if (load[s] == 0) continue;  // node không có tác vụ thì không bật (w_i >= 1 nên load > 0 khi có tác vụ)
        ++r.nodes_used;
        r.energy += v.e_on + v.exec(static_cast<ld>(load[s]));
        if (static_cast<ld>(load[s]) > v.cap) ++r.violated_cap;
    }
    r.energy += v.eps * v.d[0];
    if (v.m >= 2) {
        for (int i = 1; i <= v.n - 1; ++i) {
            r.energy += v.eps * v.d[i];
            if (v.d[i] > v.link_cap) ++r.violated_link;
        }
    }
    r.feasible = r.violated_cap == 0 && r.violated_link == 0;
    return r;
}

const std::vector<std::string>& heuristic_names() {
    static const std::vector<std::string> names = {"greedy", "equal", "single", "roundrobin"};
    return names;
}

HeuristicResult solve_heuristic(const std::string& name, const Instance& in) {
    if (name == "greedy") return solve_greedy(in);
    if (name == "equal") return solve_equal_split(in);
    if (name == "single") return solve_single_node(in);
    if (name == "roundrobin") return solve_round_robin(in);
    throw std::invalid_argument("heuristic không tồn tại: " + name);
}

}  // namespace edgedp
