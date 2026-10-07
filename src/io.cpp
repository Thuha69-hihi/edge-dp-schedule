#include "io.hpp"

namespace edgedp {

bool read_instance(std::istream& is, Instance& out) {
    int n = 0, m = 0;
    if (!(is >> n >> m) || n < 0 || m < 0) return false;
    out.work.assign(n, 0);
    out.nodes.assign(m, Node{0, 0});
    for (int i = 0; i < n; ++i)
        if (!(is >> out.work[i])) return false;
    for (int k = 0; k < m; ++k)
        if (!(is >> out.nodes[k].alpha >> out.nodes[k].setup)) return false;
    return true;
}

}  // namespace edgedp
