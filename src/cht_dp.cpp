// Convex Hull Trick (deque) -> O(M*N) khi hệ số góc và điểm truy vấn đơn điệu.
// Gợi ý: với mỗi node k,
//   dp[i] = alpha_k*P[i]^2 + setup_k + min_j( (-2*alpha_k*P[j]) * P[i] + dp[j] + alpha_k*P[j]^2 )
// => mỗi j là một đường thẳng y = a*x + b, truy vấn tại x = P[i].
// Lưu ý tràn số: so sánh giao điểm bằng __int128.
// TODO(TV3, tuần 4): cài đặt, chờ TV2 xác nhận dạng biến đổi và xử lý deadline.
#include "edgedp/solver.hpp"

namespace edgedp {

Solution solve_cht(const Instance&) {
    throw NotImplemented("solve_cht: chua cai dat (TV3, tuan 4)");
}

}  // namespace edgedp
