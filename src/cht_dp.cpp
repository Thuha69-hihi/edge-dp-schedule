// Convex Hull Trick -> O(M*N) nếu hệ số góc và điểm truy vấn đơn điệu.
//
// LƯU Ý QUAN TRỌNG (mục 5 tài liệu mô hình): với mô hình bậc ba (S_i - S_k)^3, khai triển có các số hạng
// S_i^2*S_k và S_i*S_k^2, nên CHT chuẩn KHÔNG dùng được. CHT chỉ dùng cho biến thể bậc hai
//   E_exec(W) = a*W^2 + b*W :
//   DP(i,j) = a*S_i^2 + b*S_i + E_on + min_k { (-2a*S_k) * S_i + [DP(k,j-1) + a*S_k^2 - b*S_k + eps*d_k] }
// Hệ số góc -2a*S_k giảm dần theo k, điểm truy vấn S_i tăng dần -> deque, O(1) khấu hao.
//
// Cần xử lý khi cài (việc 3.02 của TV2): ràng buộc S_i - S_k <= cap tạo cửa sổ trượt trên k (bỏ đường hết hạn
// khỏi đầu deque), và ràng buộc d_k <= link_cap loại một số k rời rạc. Nếu không chứng minh được thì dùng
// Li Chao tree. So sánh giao điểm bằng phép nhân chéo, chú ý tràn số.
// Chờ quyết định ở buổi họp tuần 1 (mục 9, câu 1): bậc ba hay bậc hai, tham số hóa thế nào.
// TODO(TV3, tuần 4): cài đặt.
#include "edgedp/solver.hpp"

namespace edgedp {

Solution solve_cht(const Instance&) {
    throw NotImplemented("solve_cht: chua cai dat (TV3, tuan 4)");
}

}  // namespace edgedp
