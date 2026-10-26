// Convex Hull Trick -> O(M*N) nếu hệ số góc và điểm truy vấn đơn điệu.
//
// LƯU Ý QUAN TRỌNG (mục 5 tài liệu mô hình): với mô hình bậc ba (S_i - S_k)^3, khai triển có các số hạng
// S_i^2*S_k và S_i*S_k^2, nên CHT chuẩn KHÔNG dùng được. CHT chỉ dùng cho biến thể bậc hai
//   E_exec(W) = a*W^2 + b*W :
//   DP(i,j) = a*S_i^2 + b*S_i + E_on + min_k { (-2a*S_k) * S_i + [DP(k,j-1) + a*S_k^2 - b*S_k + eps*d_k] }
// Hệ số góc -2a*S_k giảm dần theo k, điểm truy vấn S_i tăng dần -> deque, O(1) khấu hao.
//
#pragma once
#include <vector>
#include <deque>
#include <algorithm>
#include "types.hpp"
#include "cost.hpp"

namespace edgedp {

// Cấu trúc đại diện cho đường thẳng y = m * x + c
struct Line {
    ld m, c;
    int k; // Lưu lại chỉ số k để phục vụ truy vết điểm cắt

    ld eval(ld x) const {
        return m * x + c;
    }

    // Tính tọa độ x là giao điểm của đường thẳng hiện tại với đường thẳng next Line
    ld intersect(const Line& other) const {
        return (other.c - c) / (m - other.m);
    }
};

class CHTSolver {
private:
    const Instance& in;
    CostModel cost_model;

public:
    explicit CHTSolver(const Instance& instance) 
        : in(instance), cost_model(instance) {}

    // Hàm giải bài toán DP với CHT, trả về chi phí tối ưu và vị trí cắt để truy vết
    std::pair<ld, std::vector<int>> solve(int M) {
        int n = in.n;
        std::vector<std::vector<ld>> dp(M + 1, std::vector<ld>(n + 1, inf()));
        std::vector<std::vector<int>> parent(M + 1, std::vector<int>(n + 1, -1));

        dp[0][0] = 0;

        // Giả định hệ số quadratic 'a' cho mô hình bậc hai E_exec = a * W^2
        // Lấy hệ số a từ tham số params
        ld a = in.params.kappa_over_T2; 

        for (int j = 1; j <= M; ++j) {
            std::deque<Line> cht;

            for (int k = 0; k < n; ++k) {
                // Chỉ thêm vào CHT các trạng thái k hợp lệ ở bước (j-1)
                if (dp[j - 1][k] < inf()) {
                    ld S_k = static_cast<ld>(cost_model.S[k]);
                    
                    // Với E_exec = a * (S_i - S_k)^2 = a * S_i^2 - 2a * S_k * S_i + a * S_k^2
                    // Line: m = -2 * a * S_k, c = dp[j-1][k] + a * S_k^2 + e_on + eps * d[k]
                    ld m_val = -2.0 * a * S_k;
                    ld c_val = dp[j - 1][k] + a * S_k * S_k + in.params.e_on;
                    if (k >= 1) {
                        c_val += in.params.eps * static_cast<ld>(in.d[k]);
                    }

                    Line new_line = {m_val, c_val, k};

                    // Loại bỏ các đường thẳng không thuộc Convex Hull ở cuối deque
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

                // Truy vấn tại x = S_i (với i = k + 1)
                int i = k + 1;
                ld S_i = static_cast<ld>(cost_model.S[i]);

                if (!cht.empty()) {
                    // Loại bỏ các đường thẳng ở đầu deque không còn tối ưu cho S_i
                    while (cht.size() >= 2 && cht[0].eval(S_i) >= cht[1].eval(S_i)) {
                        cht.pop_front();
                    }

                    // Kiểm tra các ràng buộc cap và link_cap
                    int best_k = cht.front().k;
                    ld W = static_cast<ld>(cost_model.work(best_k, i));

                    if (W <= in.params.cap && (best_k == 0 || static_cast<ld>(in.d[best_k]) <= in.params.link_cap)) {
                        ld current_cost = cht.front().eval(S_i) + a * S_i * S_i;
                        if (best_k == 0) {
                            current_cost += in.params.eps * static_cast<ld>(in.d[0]);
                        }
                        
                        dp[j][i] = current_cost;
                        parent[j][i] = best_k;
                    }
                }
            }
        }

        // Truy vết nghiệm cắt đoạn
        std::vector<int> cuts;
        int curr = n;
        int curr_j = M;
        while (curr > 0 && curr_j > 0) {
            int p = parent[curr_j][curr];
            if (p == -1) break;
            cuts.push_back(p);
            curr = p;
            curr_j--;
        }
        std::reverse(cuts.begin(), cuts.end());

        return {dp[M][n], cuts};
    }
};

} // namespace edgedp