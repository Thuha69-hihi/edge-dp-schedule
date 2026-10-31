# Kịch bản demo (7.05)

Thời lượng dự kiến: 3–4 phút. Chạy từ thư mục gốc repo (đã `git checkout v1.0` hoặc `main`).
Chuẩn bị trước (làm một lần, ngoài giờ trình bày): `make` và kiểm tra `bash scripts/demo.sh` chạy được.

## Lệnh chạy
```
bash scripts/demo.sh            # demo chính, khoảng 2 giây
FULL=1 bash scripts/demo.sh     # thêm baseline ở N = 10^5, khoảng 1 phút (dùng cho video)
```

## Diễn biến và lời dẫn
| Bước | Màn hình | Lời dẫn gợi ý |
|---|---|---|
| 1 | N = 10^4, M = 16: baseline ≈ 0,5 s, D&C ≈ 10 ms, **cùng năng lượng** | "Hai thuật toán cho đúng một đáp án; D&C nhanh hơn khoảng 50 lần ở N = 10^4." |
| 2 | N = 10^5, M = 16: D&C ≈ 0,13 s. Baseline (video) ≈ 58 s | "Khi N tăng 10 lần, baseline chậm khoảng 100 lần (O(M·N²)), D&C chỉ khoảng 13 lần (O(M·N log N))." |
| 3 | Mô hình bậc hai, N = 10^5: D&C và CHT cùng năng lượng | "CHT chỉ dùng được với mô hình bậc hai; khi cap cắt cửa sổ nó dùng cây phân đoạn nên chậm hơn D&C." |

## Số liệu tham chiếu (máy 2 nhân, g++ -O2, file `syn_*_m16_a1.2_b10_s1`)
| Solver | N = 10^4 | N = 10^5 |
|---|---|---|
| baseline | ≈ 0,5 s | ≈ 58 s |
| D&C | ≈ 10 ms | ≈ 130 ms |
| CHT (bậc hai) | – | ≈ 300 ms |
Số trên máy nhóm có thể khác; lấy số liệu chính thức từ `results/time_mem.csv`.

## Video dự phòng
1. Đặt cỡ chữ terminal lớn, xóa màn hình trước khi quay (`clear`).
2. Quay màn hình khi chạy `FULL=1 bash scripts/demo.sh` từ đầu đến cuối (khoảng 1 phút 30 giây).
3. Lưu `demo_backup.mp4` và gửi vào thư mục chung của nhóm.

## Rủi ro khi demo trực tiếp
* Máy trình bày không có g++ → build sẵn trước, hoặc dùng video dự phòng.
* Baseline N = 10^5 mất khoảng 1 phút → mặc định không chạy trực tiếp, chỉ chiếu video.
* Thiếu `data/synthetic_quadratic`
