# Quy ước làm việc (theo sheet KeHoach_Nhom2_DP_EdgeIoT)

- **Không push thẳng `main`.** Tạo nhánh từ main, mở Pull Request, cần ít nhất 1 người review.
- Tên nhánh: `<mã TV>/<việc>` ví dụ `tv3/baseline-dp`, `tv4/dc-opt`, `tv5/benchmark`.
- Commit message ngắn, rõ: `baseline: them truy vet nghiem`, `tests: them test deadline`.
- Trước khi mở PR: `make test` không được có FAIL. SKIP là chấp nhận được nếu solver chưa cài.
- Việc xong thì dán link commit/PR vào cột "Link minh chứng" trong sheet.

## Thêm một thuật toán mới
1. Cài trong `src/` với đúng chữ ký `Solution solve_xxx(const Instance&)`.
2. Khai báo trong `include/edgedp/solver.hpp` và thêm vào `src/registry.cpp`.
3. Test đối chiếu với baseline tự động chạy qua `cross_check_*` trong `tests/test_main.cpp`.

## Đổi mô hình bài toán
Chỉ sau khi TV1 chốt trong tài liệu "Phát biểu hình thức bài toán (1.05)". Sửa tài liệu đó, rồi `docs/model.md`,
`types.hpp`, `cost.hpp`, báo cả nhóm, rồi mới sửa solver. Mọi solver phải dùng `CostModel::cost` chung, không tự viết lại công thức.
