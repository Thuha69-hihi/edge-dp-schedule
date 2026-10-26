# Edge-DP-Scheduler

Nhóm 2, môn Phân tích thiết kế thuật toán (NEU).
Đề tài: **Quy hoạch động cải tiến cho lập lịch tác vụ tiết kiệm năng lượng trên Edge/IoT**
(D&C Optimization, Convex Hull Trick).

Bài toán: cắt chuỗi N tác vụ thành tối đa M đoạn liên tiếp, mỗi đoạn chạy trên một Edge node,
sao cho tổng năng lượng (tính toán + truyền dữ liệu + bật node) nhỏ nhất mà mỗi node xử lý xong trong chu kỳ T.
Chi tiết: [docs/model.md](docs/model.md).

## Cấu trúc

```
include/edgedp/   types.hpp, cost.hpp, solver.hpp, heuristics.hpp
src/              baseline_dp, cht_dp, dc_opt, brute_force, heuristics, json, io, main, main_heuristics
bench/            run_bench.cpp (đo thời gian & bộ nhớ RSS)
tests/            test_main.cpp (unit test), data/example_n5.json (ví dụ mục 6 của tài liệu)
scripts/          gen_data.py (sinh dữ liệu IoT tổng hợp)
docs/             model.md, CONTRIBUTING.md
.github/          CI và mẫu Pull Request
```

## Yêu cầu
- Trình biên dịch hỗ trợ C++17 (g++ 9+, clang) và `make`; hoặc CMake 3.14+
- Python 3 (chỉ để sinh dữ liệu)

## Build và test

```bash
make          # build ./build/edgedp, ./build/edgedp_heur, ./build/run_bench và ./build/run_tests
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

### Sinh dữ liệu và benchmark

#### 1. Sinh dữ liệu IoT mô phỏng (`scripts/gen_data.py`)

```bash
# Sinh 1 instance mẫu
python3 scripts/gen_data.py --n 1000 --m 8 --seed 1 --alpha 4 --beta 10 --out data/x.json

# Lưới nhỏ để thử nghiệm
python3 scripts/gen_data.py --grid --ns 1000 10000 --seeds 1 2 3 --out-dir data/small

# Sinh toàn bộ lưới thí nghiệm
python3 scripts/gen_data.py --grid --out-dir data/synthetic
```

#### 2. So sánh Heuristic với DP baseline (`edgedp_heur`)

```bash
# Chạy 4 heuristic (greedy, equal, single, roundrobin) và đối chiếu với DP baseline
./build/edgedp_heur --input data/x.json
```

#### 3. Chạy benchmark thời gian và bộ nhớ (`run_bench` trên Linux)

```bash
# Benchmark trên 1 instance
./build/run_bench --bin ./build/edgedp data/x.json --out results/time_mem.csv

# Hoặc chạy đo hàng loạt theo manifest.csv
./build/run_bench --bin ./build/edgedp data/small/manifest.csv \
    --solvers baseline,dc,cht --repeats 3 --timeout 600 \
    --max-n baseline=10000 \
    --out results/time_mem.csv --summary results/time_mem_summary.csv
```

* **Đầu vào:** Nhận file `.json`, thư mục chứa các file json, glob pattern hoặc `manifest.csv` do script sinh dữ liệu tạo ra.
* **Đo bộ nhớ:** Khuyến nghị cài `/usr/bin/time` (`sudo apt install time` trên Ubuntu) để đo đỉnh bộ nhớ (Peak RSS) chính xác nhất. Nếu thiếu, chương trình tự động chuyển sang đọc `/proc`.
* **Đối chiếu nghiệm:** Các solver chính xác (`baseline`, `dc`, `cht`) được tự động kiểm tra xem có cho cùng giá trị năng lượng hay không (`match_ref = ok` hoặc `MISMATCH`). Heuristic được tính phần trăm tiết kiệm năng lượng so với DP baseline.

## Đóng góp
Xem [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md): không push thẳng `main`, mọi thay đổi qua Pull Request có review.
