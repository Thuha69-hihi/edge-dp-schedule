#pragma once
#include <istream>
#include <string>

#include "edgedp/types.hpp"

namespace edgedp {

// Đọc input JSON theo mục 8 của tài liệu mô hình:
//   {"n":5,"m":3,"w":[...],"d":[...],"params":{"kappa_over_T2":..,"e_on":..,"eps":..,"cap":..,"link_cap":..}}
// Trường tùy chọn: "seed", "source". Ném std::runtime_error nếu thiếu trường hoặc sai kích thước.
Instance read_instance_json(std::istream& is);
Instance parse_instance_json(const std::string& text);

// Ghi output JSON theo mục 8 (energy = null nếu không khả thi).
std::string solution_to_json(const Solution& s);

}  // namespace edgedp
