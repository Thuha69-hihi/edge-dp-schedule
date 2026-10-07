# Mô hình bài toán (chờ TV1 chốt)

## Mô hình tạm đang dùng trong code (v0)
- Có N tác vụ xếp thành chuỗi tuần tự, khối lượng `w[i]`. Gọi `P[i]` là tổng tiền tố.
- Có M node, node k có `alpha_k` và `setup_k`.
- Chia chuỗi thành các nhóm liên tiếp; nhóm `(j, i]` chạy trên node k tốn
  `alpha_k * (P[i] - P[j])^2 + setup_k` năng lượng.
- Mục tiêu: tổng năng lượng nhỏ nhất.

Công thức truy hồi:  `dp[i] = min_{j<i, k} dp[j] + alpha_k*(P[i]-P[j])^2 + setup_k`, `dp[0] = 0`.

## Chưa có (cần TV1/TV2 quyết định)
- [ ] Ràng buộc deadline và cách đưa vào DP
- [ ] Thời gian chạy / truyền dữ liệu của từng node
- [ ] Mô hình năng lượng chính thức (có đúng bậc 2 theo khối lượng không)
- [ ] Điều kiện để Quadrangle Inequality / CHT áp dụng được

## Ghi chú kiểu dữ liệu
Dùng `long long`. Với work lớn, `alpha * W^2` có thể tràn; kiểm tra khi benchmark N lớn.
