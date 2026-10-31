// Kiểu dữ liệu dùng chung cho mọi thuật toán (baseline, CHT, D&C, brute force).

// Quy ước chỉ số (rất dễ nhầm, đọc kỹ):
//   - w[0..n-1] chứa w_1..w_N       (w[i-1] là khối lượng của tác vụ τ_i)
//   - d[0..n-1] chứa d_0..d_{N-1}   (d[k] là dữ liệu truyền sau tác vụ τ_k, d[0] là từ thiết bị IoT)
//   - S[0..n] là tổng tiền tố, S[0] = 0, S[i] = w_1 + ... + w_i
//   - cost(k, i) nhận chỉ số tiền tố k: đoạn gồm τ_{k+1}..τ_i (xem cost.hpp)
#pragma once
#include <stdexcept>
#include <string>
#include <vector>

namespace edgedp {

using ll = long long;    // w, S, d, chỉ số
using ld = long double;  // năng lượng (đã chốt đề xuất ở mục 9, câu 4)

// Mô hình năng lượng thực thi E_exec(W) của một đoạn có khối lượng W (quyết định họp tuần 1, mục 9 câu 1):
//   Cubic     : E_exec(W) = (κ/T²)·W³        (DVFS, mô hình chính, dùng D&C)
//   Quadratic : E_exec(W) = a·W² + b·W      (biến thể, a >= 0, để dùng được Convex Hull Trick)
enum class EnergyModel { Cubic, Quadratic };

struct Params {
    ld kappa_over_T2 = 1.0;  // κ / T^2 (chỉ dùng khi model = Cubic)
    ld e_on = 0.0;           // E_on: năng lượng bật một node trong một chu kỳ
    ld eps = 0.0;            // ε = P_tx / B (J/bit)
    ld cap = 0.0;            // f_max * T: khối lượng tối đa của một đoạn
    ld link_cap = 0.0;       // B * T: dữ liệu tối đa truyền qua một chỗ cắt
    // Thêm ở cuối để các chỗ khởi tạo theo thứ tự {κ/T², E_on, ε, cap, link_cap} vẫn đúng.
    EnergyModel model = EnergyModel::Cubic;
    ld quad_a = 0.0;  // a (chỉ dùng khi model = Quadratic), a >= 0
    ld quad_b = 0.0;  // b (chỉ dùng khi model = Quadratic)
};

struct Instance {
    int n = 0;          // số tác vụ N
    int m = 0;          // số node tối đa M
    std::vector<ll> w;  // size n
    std::vector<ll> d;  // size n
    Params params;
    // Tùy chọn, để tái tạo thí nghiệm (bộ sinh dữ liệu ghi vào, solver bỏ qua).
    std::string source;
    ll seed = 0;
};

struct Segment {
    int from;  // chỉ số tác vụ bắt đầu (tính từ 1)
    int to;    // chỉ số tác vụ kết thúc
    ll work;   // S_to - S_{from-1}
    ld cost;
};

struct Solution {
    bool feasible = false;
    ld energy = 0;                  // chỉ có nghĩa khi feasible
    int nodes_used = 0;             // j
    std::vector<int> cuts;          // c_1..c_{j-1}, chỉ số tính từ 1
    std::vector<Segment> segments;  // j đoạn
    std::string solver;             // CLI điền
    double time_ms = 0;             // CLI điền
};

// Ném ra khi một thuật toán chưa được cài đặt (test sẽ báo SKIP thay vì FAIL).
struct NotImplemented : std::logic_error {
    explicit NotImplemented(const char* what) : std::logic_error(what) {}
};

}  // namespace edgedp
