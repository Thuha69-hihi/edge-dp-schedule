// Heuristic baseline để tính Energy Saved % (việc 2.08, TV5 Thành Công).
// DP gốc và DP cải tiến cho cùng nghiệm tối ưu nên không so với nhau về năng lượng;
// năng lượng của DP được so với 4 heuristic dưới đây (xem sheet Rủi ro, mục 4).
#pragma once
#include <string>
#include <vector>

#include "edgedp/solver.hpp"  // Instance, ld

namespace edgedp {

// Kết quả một heuristic. Heuristic KHÔNG biết deadline thì có thể vi phạm, nên ngoài
// energy còn báo cáo số vi phạm để tính "tỷ lệ vi phạm deadline" (việc 5.04).
struct HeuristicResult {
    std::string name;
    bool feasible = false;  // đủ mọi ràng buộc: khối lượng <= cap, d <= link_cap, <= m node
    ld energy = 0;          // E_total theo công thức mô hình, tính cả khi vi phạm ràng buộc
    int nodes_used = 0;
    int violated_cap = 0;   // số node có khối lượng > cap (f > f_max)
    int violated_link = 0;  // số chỗ truyền có d > link_cap
    std::vector<int> cuts;  // điểm cắt c_1..c_{j-1}; rỗng với round-robin (không liên tiếp)
};

// Tên: "greedy", "equal", "single", "roundrobin".
HeuristicResult solve_greedy(const Instance& in);       // first-fit theo deadline
HeuristicResult solve_equal_split(const Instance& in);  // chia đều số tác vụ cho min(M, N) node
HeuristicResult solve_single_node(const Instance& in);  // chạy toàn bộ trên 1 node
HeuristicResult solve_round_robin(const Instance& in);  // tác vụ i -> node (i-1) mod M

const std::vector<std::string>& heuristic_names();
HeuristicResult solve_heuristic(const std::string& name, const Instance& in);  // ném nếu sai tên

}  // namespace edgedp
