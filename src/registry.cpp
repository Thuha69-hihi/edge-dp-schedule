#include "edgedp/solver.hpp"

namespace edgedp {

const std::vector<SolverEntry>& all_solvers() {
    static const std::vector<SolverEntry> v = {
        {"baseline", solve_baseline},
        {"cht", solve_cht},
        {"dc", solve_dc},
        {"bruteforce", solve_bruteforce},
    };
    return v;
}

}  // namespace edgedp
