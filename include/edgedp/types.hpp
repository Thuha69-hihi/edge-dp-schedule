// Kiểu dữ liệu dùng chung cho mọi thuật toán (baseline, CHT, D&C, brute force).
// TV1 chốt mô hình chính thức -> nếu đổi mô hình thì sửa file này TRƯỚC, rồi mới sửa solver.
#pragma once
#include <stdexcept>
#include <vector>

namespace edgedp {

using ll = long long;

// Một node biên (hoặc một mức cấu hình).
// Mô hình tạm (v0): năng lượng chạy một nhóm có tổng khối lượng W là
//     alpha * W^2 + setup
// TODO(TV1): thay bằng mô hình chính thức, bổ sung deadline.
struct Node {
    ll alpha;  // hệ số năng lượng theo bình phương khối lượng
    ll setup;  // chi phí cố định mỗi lần gửi/khởi động
};

struct Instance {
    std::vector<ll> work;     // work[i] = khối lượng tác vụ i (chuỗi tuần tự, i = 0..N-1)
    std::vector<Node> nodes;  // M node
};

// Một nhóm tác vụ liên tiếp [begin, end) chạy trên node `node`.
struct Batch {
    int begin;
    int end;
    int node;
};

struct Solution {
    bool feasible = false;
    ll energy = 0;
    std::vector<Batch> batches;
};

// Ném ra khi một thuật toán chưa được cài đặt (test sẽ báo SKIP thay vì FAIL).
struct NotImplemented : std::logic_error {
    explicit NotImplemented(const char* what) : std::logic_error(what) {}
};

}  // namespace edgedp
