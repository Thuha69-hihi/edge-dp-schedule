#pragma once
#include <istream>

#include "edgedp/types.hpp"

namespace edgedp {
// Định dạng file input:
//   dòng 1: N M
//   dòng 2: N số nguyên work[0..N-1]
//   M dòng tiếp: alpha setup (của từng node)
bool read_instance(std::istream& is, Instance& out);
}  // namespace edgedp
