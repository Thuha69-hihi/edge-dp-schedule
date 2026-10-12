# Edge-DP-Scheduler

Nhóm 2, môn Phân tích thiết kế thuật toán (NEU).
Đề tài: **Quy hoạch động cải tiến cho lập lịch tác vụ tiết kiệm năng lượng trên Edge/IoT**
(D&C Optimization, Convex Hull Trick).

Bài toán: cắt chuỗi N tác vụ thành tối đa M đoạn liên tiếp, mỗi đoạn chạy trên một Edge node,
sao cho tổng năng lượng (tính toán + truyền dữ liệu + bật node) nhỏ nhất mà mỗi node xử lý xong trong chu kỳ T.
Chi tiết: [docs/model.md](docs/model.md).

## Cấu trúc

```
include/edgedp/   types.hpp (kiểu dữ liệu), cost.hpp (hàm chi phí dùng chung), solver.hpp (giao diện solver)
src/              baseline_dp, cht_dp, dc_opt, brute_force, json, io, main (CLI)
tests/            test_main.cpp (unit test), data/example_n5.json (ví dụ mục 6 của tài liệu)
scripts/          gen_data.py (sinh dữ liệu tạm), run_benchmark.sh
docs/             model.md, CONTRIBUTING.md
.github/          CI và mẫu Pull Request
```

## Yêu cầu
- Trình biên dịch hỗ trợ C++17 (g++ 9+, clang) và `make`; hoặc CMake 3.14+
- Python 3 (chỉ để sinh dữ liệu)

## Build và test

```bash
make          # build ./build/edgedp và ./build/run_tests
make test     # chạy unit test
make clean
```

Hoặc dùng CMake: `cmake -S . -B build && cmake --build build && ctest --test-dir build`

## Chạy

```bash
./build/edgedp --algo baseline --input tests/data/example_n5.json
```

Kết quả mong đợi: `energy` = 973, `nodes_used` = 2, `cuts` = [2].
Thuật toán: `baseline`, `cht`, `dc`, `bruteforce` (cái nào chưa cài sẽ báo "chua cai dat").

### Định dạng vào/ra (JSON)

Đầu vào:

```json
{
  "n": 5, "m": 3,
  "w": [2, 3, 1, 4, 2],
  "d": [3, 4, 2, 6, 1],
  "params": {"kappa_over_T2": 1.0, "e_on": 250.0, "eps": 1.0, "cap": 8.0, "link_cap": 5.0}
}
```

`w` có N phần tử (w₁..w_N), `d` có N phần tử (d₀..d_{N−1}), `cap` = f_max·T, `link_cap` = B·T.
Có thể thêm `seed`, `source` để tái tạo thí nghiệm.

Đầu ra: `feasible`, `energy` (null nếu không khả thi), `nodes_used`, `cuts`, `segments`, `solver`, `time_ms`.
Mọi solver phải cho cùng `energy` (sai số ≤ 1e-9); `cuts` có thể khác khi có nhiều nghiệm tối ưu.

### Sinh dữ liệu và benchmark thử
2. Build CLI và harness

```bash
mkdir -p build
g++ -std=c++17 -O2 -Iinclude -Isrc src/*.cpp -o build/edgedp
g++ -std=c++17 -O2 -Isrc bench/run_bench.cpp src/json.cpp -o build/run_bench
```
3. Sinh dữ liệu
```
bash
# một instance
python3 scripts/gen_data.py --n 1000 --m 8 --seed 1 --alpha 4 --beta 10 --out data/x.json

# cả lưới thí nghiệm
python3 scripts/gen_data.py --grid --out-dir data/synthetic

# lưới nhỏ để thử
python3 scripts/gen_data.py --grid --ns 1000 10000 --seeds 1 2 3 --out-dir data/small
```
5. Chạy benchmark

```bash
./build/run_bench --bin build/edgedp data/small/manifest.csv \
    --solvers baseline,dc,cht --repeats 3 --timeout 600 \
    --max-n baseline=10000 \
    --out results/time_mem.csv --summary results/time_mem_summary.csv
```
Đầu vào: có thể là file .json, thư mục, glob (ví dụ "data/synthetic/*_n1000_*.json") hoặc manifest.csv.
Đo bộ nhớ: nên cài /usr/bin/time (sudo apt install time) để có RSS chính xác, harness sẽ tự dùng nó. Nếu thiếu, nó chuyển sang đọc /proc, kém chính xác hơn với lần chạy dưới khoảng 10 ms. Bạn có thể ép chế độ bằng --mem-mode time|proc|none.
Solver chưa cài: dc và cht hiện chỉ là stub nên hiện dòng not_impl, không tính vào đối chiếu. Khi cài xong thì không cần sửa gì.
Mã thoát: 0 là bình thường. 3 nghĩa là các solver chính xác cho năng lượng khác nhau (có dòng MISMATCH trong CSV). 2 là sai tham số hoặc không tìm thấy CLI.

```bash
python3 scripts/gen_data.py --n 1000 --m 8 --seed 1 > data/generated/n1000.json
sh scripts/run_benchmark.sh baseline
```

## Đóng góp
Xem [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md): không push thẳng `main`, mọi thay đổi qua Pull Request có review.
