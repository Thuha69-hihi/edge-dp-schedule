// Giao diện chung: mọi solver nhận Instance và trả Solution.
// Giữ chữ ký này để TV4 đối chiếu và TV5 benchmark được mà không sửa code.
#pragma once
#include <vector>

#include "types.hpp"

namespace edgedp {

Solution solve_baseline(const Instance& in);    // DP gốc O(M*N^2)    - TV3
Solution solve_cht(const Instance& in);         // Convex Hull Trick  - TV3
Solution solve_dc(const Instance& in);          // D&C Optimization   - TV4
Solution solve_bruteforce(const Instance& in);  // vét cạn, N nhỏ     - TV4

using SolverFn = Solution (*)(const Instance&);

struct SolverEntry {
    const char* name;
    SolverFn fn;
};

// Danh sách solver để CLI, test và benchmark dùng chung.
const std::vector<SolverEntry>& all_solvers();

}  // namespace edgedp
