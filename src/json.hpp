// JSON tối giản, chỉ đủ để đọc input của nhóm. Không dùng cho mục đích khác.
#pragma once
#include <string>
#include <utility>
#include <vector>

namespace edgedp {

struct Json {
    enum Type { Null, Bool, Num, Str, Arr, Obj } type = Null;
    bool b = false;
    long double num = 0;
    std::string str;
    std::vector<Json> arr;
    std::vector<std::pair<std::string, Json>> obj;

    const Json* get(const std::string& key) const;  // nullptr nếu không có
};

// Ném std::runtime_error nếu sai cú pháp.
Json parse_json(const std::string& text);

}  // namespace edgedp
