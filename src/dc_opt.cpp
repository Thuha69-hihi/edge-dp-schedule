// Divide and Conquer Optimization -> O(M * N log N) vì opt(i, j) <= opt(i+1, j) (QI, mục 5 tài liệu mô hình).
// Mỗi hàng j: compute(lo, hi, optlo, opthi) tính DP(mid, j) với k trong [optlo, min(mid-1, opthi)].
// Cẩn thận (đưa cho TV1/TV2 xác nhận): Cost có +inf (vượt cap, d_k > link_cap) và DP(k, j-1) cũng có +inf.
// Khi mọi k trong khoảng đều cho +inf, opt bị tùy ý và có thể phá tính đơn điệu -> cần quy tắc rõ ràng
// (ví dụ giữ nguyên cận khi không tìm được k hữu hạn) và test đối chiếu với baseline trên input vô nghiệm/gần vô nghiệm.
// TODO(TV4, tuần 4): cài đặt.
#include "edgedp/solver.hpp"

namespace edgedp {

Solution solve_dc(const Instance&) {
    throw NotImplemented("solve_dc: chua cai dat (TV4, tuan 4)");
}

}  // namespace edgedp
