# Ôn tập kỹ thuật tối ưu DP (việc 1.07 của TV4)

Ghi chú nội bộ ôn ba kỹ thuật tối ưu DP sẽ áp dụng trong đề tài
(Convex Hull Trick, Divide & Conquer Optimization, Knuth Optimization).
Mỗi kỹ thuật có: điều kiện áp dụng, công thức, ví dụ nhỏ chạy được.

Công thức DP gốc của đề tài (mục 4 tài liệu 1.05):

```
DP(i, j) = min_{j-1 <= k < i} { DP(k, j-1) + Cost(k+1, i) },   DP(0,0)=0
E*      = min_{1 <= j <= M} DP(N, j)
```

Với Cost thỏa bất đẳng thức tứ giác (QI), nghĩa là `opt(i, j) đơn điệu không giảm`.

---

## 1. Convex Hull Trick (CHT)

### Khi nào dùng được
Sau khai triển `DP(i, j) = const(i) + min_k { m_k · x_i + b_k }` trong đó hệ số góc `m_k`
đơn điệu (tăng hoặc giảm theo k) và điểm truy vấn `x_i` đơn điệu (cùng chiều với m_k).
Đường `f_k(x) = m_k · x + b_k` lưu trong:
- **deque** nếu hệ số góc đơn điệu và truy vấn đơn điệu → O(1) amortized cho cả `add_line`
  và `query(x)`.
- **Li Chao tree** nếu thứ tự truy vấn không đơn điệu → O(log X) mỗi thao tác.

### Áp dụng cho đề tài
Mô hình bậc ba `(S_i − S_k)^3` khai triển có các số hạng `S_i²·S_k` và `S_i·S_k²`,
không viết được thành một đường thẳng theo một biến nên **CHT chuẩn không dùng được**
cho mô hình chính. Trong biến thể bậc hai (kỳ vọng E_exec(W) = a·W² + b·W), ta có:

```
DP(i, j) = a S_i^2 + b S_i + E_on
          + min_k { (-2a S_k) · S_i + [DP(k, j-1) + a S_k^2 - b S_k + ε d_k] }
```

Hệ số góc `-2a S_k` giảm dần theo k (vì S_k tăng), điểm truy vấn `S_i` tăng dần theo i
→ deque, O(1) amortized → tổng O(M·N).

### Ví dụ nhỏ (độc lập, không phụ thuộc repo)
`scripts/chong_mat_deque.py` (đoạn trích dưới) minh họa deque CHT cổ điển:
```python
from collections import deque

def cht_query(deq, x):
    while len(deq) >= 2 and deq[0].value(x) >= deq[1].value(x):
        deq.popleft()
    return deq[0].value(x)

def cht_add(deq, m, b):
    new = Line(m, b)
    while len(deq) >= 2 and deq[-2].intersect(new) <= deq[-2].intersect(deq[-1]):
        deq.pop()
    deq.append(new)
```
Với bài toán nhỏ, ta có thể thay bằng kiểm tra giao điểm bằng phép nhân chéo để tránh
tràn số.

---

## 2. Divide & Conquer Optimization (D&C)

### Điều kiện (Quadrangle Inequality / Monge)
Với mọi `a ≤ b ≤ c ≤ d`:
```
Cost(a, c) + Cost(b, d) ≤ Cost(a, d) + Cost(b, c)
```
Kéo theo `opt(i, j) ≤ opt(i+1, j)` (đơn điệu không giảm).

### Thuật toán
Với hàng `j`:
```
compute(j, lo, hi, optL, optR):
    if lo > hi: return
    mid = (lo + hi) // 2
    best = +inf; arg = -1
    for k in [max(j-1, optL) .. min(mid-1, optR)]:
        if prev[k] + cost(k, mid) < best:
            best = prev[k] + cost(k, mid); arg = k
    cur[mid] = best; cur_opt[mid] = arg
    compute(j, lo, mid-1, optL, arg)
    compute(j, mid+1, hi, arg, optR)
```
Độ phức tạp: mỗi hàng `O(N log N)` → tổng `O(M N log N)`.

### Áp dụng cho đề tài (đã cài trong `src/dc_opt.cpp`)
Mô hình DVFS bậc ba: `Cost(k+1, i) = E_on + (κ/T²)·(S_i − S_k)³ + ε·d_k`.
Đặt `g(W) = (κ/T²)·W³` cho `W = S_i − S_k`; `g` lồi, đơn điệu tăng.
Đặt `h(k) = ε·d_k`. Viết `Cost(k+1, i) = E_on + g(S_i − S_k) + h(k)`.
- E_on triệt tiêu ở hai vế QI.
- Với `a ≤ b ≤ c ≤ d`, đặt `x = S_c − S_b`, `y = S_b − S_a`, `z = S_d − S_c` ≥ 0.
  Phần g của QI trở thành `g(x+y) + g(x+z) ≤ g(x+y+z) + g(x)`. Đúng vì `g` lồi:
  `g(x+y) − g(x) ≤ g(x+z+y) − g(x+z)`.
- Phần `h(a) + h(b) ≤ h(a) + h(b)` hiển nhiên (vế trái `h(a)+h(b)`, vế phải `h(a)+h(b)`
  vì h chỉ phụ thuộc k).
- Trường hợp +∞: nếu vế phải hữu hạn thì `x+y+z ≤ cap`, suy ra mọi đoạn con cũng ≤ cap.
- Trường hợp `d_k > link_cap` (k ≥ 1): vế phải loại b,k tương ứng → chỉ cần đảm bảo QI áp
  dụng cho phần g vẫn đúng.

→ QI thoả → `opt(i, j) ≤ opt(i+1, j)` → áp dụng D&C. Đã cài.

### Lưu ý khi cài
- `cost(k,i) = +inf` khi vượt cap hoặc `d_k > link_cap`. Trong `compute`, khi duyệt k
  mà gặp `prev[k] = +inf` thì bỏ qua. Khi **không có** k hữu hạn, gán `cur[i] = +inf`,
  `cur_opt[i] = -1` và dùng khoảng hẹp (`arg` ở vị trí lân cận) cho đệ quy.
- Dùng `long double` cho năng lượng (đề xuất của TV1).

---

## 3. Knuth Optimization

### Điều kiện
Và QI thêm một mệnh đề: `Cost(b, c) ≤ Cost(a, c)` và `Cost(b, c) ≤ Cost(b, d)` với
`a ≤ b ≤ c ≤ d` (đơn điệu của Cost), nghĩa là `opt(i, j) ≤ opt(i+1, j) ≤ opt(i, j+1)`.
Khi đó ta có thể bỏ hẳn tham số `L, R` của D&C và chạy O(N·M) tổng (mỗi `i` chỉ duyệt
trong `[opt(i, j-1), opt(i+1, j)]`).

### Áp dụng cho đề tài
Với mô hình hiện tại, mệnh đề phụ KHÔNG thoả vì:
`Cost(b, c) − Cost(a, c) = g(S_c − S_b) − g(S_c − S_a) < 0` đúng (vì S_b ≥ S_a), nhưng
`Cost(b, d) − Cost(b, c) = g(S_d − S_b) − g(S_c − S_b) ≥ 0` ngược chiều với
Knuth (Knuth yêu cầu ≤ 0). Vậy Knuth KHÔNG áp dụng được cho mô hình năng lượng bậc ba.
Đề tài không triển khai Knuth.

---

## 4. Mẫu Python chạy được đặt trong `docs/`

| File | Mục đích |
|---|---|
| `docs/dp_optimizations.md` | ghi chú này |
| `docs/cht_example.py` | deque CHT, minh hoạ đơn giản |
| `docs/dc_example.py` | D&C DP cho bài toán dãy con tăng dài nhất có trọng số |
| `docs/knuth_example.py` | ví dụ nho nhỏ về Knuth (DP nối dây) |
