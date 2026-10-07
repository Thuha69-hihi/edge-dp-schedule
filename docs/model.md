# Mô hình bài toán trong code

Nguồn chính: tài liệu **"Phát biểu hình thức bài toán (1.05)"** của nhóm trưởng (Google Docs).
File này chỉ tóm tắt những gì code đang cài, để người đọc repo không phải mở tài liệu.
Nếu hai nơi khác nhau thì **tài liệu của TV1 là chuẩn**; sửa tài liệu đó trước, rồi sửa file này và code.

## Bài toán
Cắt chuỗi N tác vụ thành tối đa M đoạn liên tiếp, đoạn thứ s chạy trên node thứ s (pipeline cố định).
Tối thiểu hóa tổng năng lượng (thực thi + truyền dữ liệu + bật node) với điều kiện mỗi đoạn xong trong chu kỳ T.

## Chi phí một đoạn (cài trong `include/edgedp/cost.hpp`)

```
Cost(k+1, i) = E_on + (κ/T²) · (S_i − S_k)³ + ε · d_k
Cost = +∞ nếu S_i − S_k > cap   (cap = f_max·T)
          hoặc (k ≥ 1 và d_k > link_cap)   (link_cap = B·T)
```

## Quy hoạch động (cài trong `src/baseline_dp.cpp`)

```
DP(i, j) = min_{j−1 ≤ k < i} { DP(k, j−1) + Cost(k+1, i) },   DP(0,0) = 0,  DP(i,0) = +∞ (i>0)
E* = min_{1 ≤ j ≤ M} DP(N, j)
```

Thời gian O(M·N²). Giá trị DP giữ hai hàng (O(N)), bảng `opt` đầy đủ O(M·N) để truy vết.

## Quy ước chỉ số trong code (dễ nhầm)

| Trong tài liệu | Trong code |
|---|---|
| w₁..w_N | `w[0..n-1]`, tức `w[i-1]` là w_i |
| d₀..d_{N−1} | `d[0..n-1]`, tức `d[k]` là d_k |
| S₀..S_N | `S[0..n]` trong `CostModel` |
| Cost(k+1, i) | `cost(k, i)`, nhận chỉ số tiền tố k |

## Kiểu số
Năng lượng dùng `long double`; w, S, d dùng `long long` (mục 9, câu 4: đề xuất của nhóm trưởng).
Hai năng lượng so sánh bằng sai số tuyệt đối/tương đối 1e-9. Test so `energy`, không so `cuts`.

## Kiểm chứng ví dụ trong tài liệu (mục 6)
`tests/data/example_n5.json` cho kết quả 973, 2 node, cắt sau τ2 (đoạn 378 + 595).
Các ô trong bảng DP đã được kiểm tra bằng một chương trình độc lập, và baseline được đối chiếu
với một bản vét cạn trong `tests/test_main.cpp` trên 600 input ngẫu nhiên (có cả vô nghiệm).

## Còn chờ họp nhóm quyết định (mục 9 của tài liệu)
- [ ] Mô hình năng lượng: bậc ba (D&C) làm chính; bậc hai là biến thể để dùng CHT. Code hiện chỉ có bậc ba.
- [ ] Giữ E_on trong mục tiêu (đề xuất: giữ)
- [ ] Deadline theo đoạn (đề xuất) hay theo từng tác vụ
- [ ] Khoảng tham số khi sinh dữ liệu (TV5)

## Điểm cần chú ý khi cài các solver nhanh
- **CHT:** với bậc ba, `(S_i − S_k)³` có số hạng S_i²·S_k và S_i·S_k², nên CHT chuẩn không dùng được;
  chỉ dùng cho biến thể bậc hai. Ràng buộc `cap` tạo cửa sổ trượt trên k.
- **D&C:** `Cost` và `DP(k, j−1)` có thể bằng +∞ (vượt cap, `d_k > link_cap`, hoặc chưa đủ node).
  Khi mọi k trong khoảng đều +∞ thì opt bị tùy ý, cần quy tắc rõ để không phá tính đơn điệu.
  Test `cross_check_dc_vs_baseline` có cả input vô nghiệm và gần vô nghiệm để bắt lỗi này.
