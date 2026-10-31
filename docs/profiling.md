# Profiling và tối ưu code (5.03)

Máy đo: 2 nhân, g++ -std=c++17 -O2 (cờ mặc định của Makefile). Mỗi số là trung vị của 3 lần chạy, thời gian lấy từ `time_ms`
do chương trình tự báo; bộ nhớ là peak RSS. Dữ liệu: `data/synthetic/*_a1.2_b10_s1.json`; CHT dùng bản sao của file
N=10^5, M=32 với `"model": "quadratic"`.

## Số liệu trước / sau

| Solver | Dữ liệu | Trước (ms) | Sau (ms) | Nhanh hơn | RSS trước → sau |
|---|---|---|---|---|---|
| baseline | N=10^4, M=16 | 1809 | 499 | 3,6x | 12,9 → 12,9 MB |
| baseline | N=10^4, M=32 | 3415 | 524 | 6,5x | 12,9 → 12,9 MB |
| D&C | N=10^5, M=16 | 117 | 131 | không đổi (nhiễu) | 40,0 → 39,9 MB |
| D&C | N=10^5, M=32 | 219 | 226 | không đổi (nhiễu) | 40,0 → 39,9 MB |
| CHT (quadratic) | N=10^5, M=32 | 616 | 593 | không đổi (nhiễu) | 43,3 → 43,4 MB |

Năng lượng, điểm cắt và tính khả thi giống hệt trước khi tối ưu: so sánh 120 lượt chạy (baseline và D&C, 60 file) không có
khác biệt nào; `make test` 22 pass / 0 fail / 1 skip; unittest2 29/29.

## Phát hiện qua profiling (gprof)
* baseline: vòng trong gọi `cost(k, i)` cho mọi k, kể cả các k đầu dãy đã vượt cap (Cost = +inf). Vì lo(i) không giảm nên
  các k này luôn là một đoạn đầu liên tục.
* D&C: 72% thời gian nằm ở `Layer::compute` (đúng là phần cần có, O(M·N·log N) lần gọi cost).
* CHT: gần như toàn bộ thời gian ở cây bao lồi (dựng lại mỗi tầng + truy vấn O(log N) nút). Chậm hơn D&C khoảng 2,8 lần
  khi có cap; chỉ chạy nhanh khi cap không cắt cửa sổ (đường deque O(M·N)).
* Bộ nhớ: peak 40 MB của D&C **không đổi khi M đổi từ 4 sang 32**, tức đỉnh nằm ở bước đọc JSON (cây `Json` ~112 byte/phần tử
  cho 2·10^5 số), không phải ở bảng DP. Bảng opt chỉ (M+1)(N+1)·4 byte, ≈ 13 MB tại N=10^5, M=32.

## Thay đổi
1. baseline: vòng trong chỉ chạy trên cửa sổ [max(j-1, lo(i)), i-1] thay vì [j-1, i-1] (bỏ qua các k chắc chắn Cost = +inf).
2. baseline, D&C, CHT: bảng opt là một `vector<int>` phẳng (M+1)(N+1) thay cho `vector<vector<int>>` (ít cấp phát, liền kề).
   Hiệu quả đo được nhỏ; giữ lại vì đơn giản hơn và không đổi hành vi.

## Đã thử nhưng bỏ
* `-O3`: baseline nhanh hơn thêm ~7%, D&C/CHT không đổi -> giữ -O2.
* Lưu bản sao đường thẳng (m, c, k) liền kề trong từng nút cây bao lồi của CHT: chậm hơn (593 → 733 ms) và tốn thêm
  khoảng 100 MB (pool nhân với số tầng) -> hoàn nguyên.
* Không dùng `-march=native` / `-ffast-math`: có thể đổi kết quả dấu phẩy động, làm lệch so sánh 1e-9 giữa các solver.

## Còn có thể làm (chưa làm)
* Parser JSON: đọc mảng số thẳng vào `vector<ll>` thay vì dựng cây `Json` sẽ cắt peak RSS của D&C/CHT còn khoảng 15 MB.
* CHT khi có cap: dựng cây theo cách chỉ cập nhật phần thay đổi hoặc dùng cấu trúc khác; hiện chậm hơn D&C.
