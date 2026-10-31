# Edge-DP-Scheduler

Nhóm 2, môn Phân tích thiết kế thuật toán (NEU).
Đề tài: **Quy hoạch động cải tiến cho lập lịch tác vụ tiết kiệm năng lượng trên Edge/IoT**
(D&C Optimization, Convex Hull Trick).

[![CI](https://github.com/Thuha69-hihi/edge-dp-schedule/actions/workflows/ci.yml/badge.svg)](https://github.com/Thuha69-hihi/edge-dp-schedule/actions/workflows/ci.yml)

- [Bài toán](#bài-toán)
- [Thuật toán đã cài](#thuật-toán-đã-cài)
- [Cấu trúc repo](#cấu-trúc-repo)
- [Cài đặt](#cài-đặt)
- [Build và test](#build-và-test)
- [Chạy solver](#chạy-solver)
- [Sinh dữ liệu, heuristic và benchmark](#sinh-dữ-liệu-heuristic-và-benchmark)
- [Tái tạo toàn bộ thí nghiệm bằng một lệnh](#tái-tạo-toàn-bộ-thí-nghiệm-bằng-một-lệnh)
- [Kết quả chính](#kết-quả-chính)
- [Đóng góp](#đóng-góp)

## Bài toán

Cắt chuỗi N tác vụ phụ thuộc tuần tự thành tối đa M đoạn liên tiếp, đoạn thứ s chạy trên Edge node thứ s,
sao cho tổng năng lượng (tính toán + truyền dữ liệu giữa hai node + bật node) nhỏ nhất, với hai ràng buộc:
mỗi node xử lý xong đoạn của mình trong chu kỳ T (khối lượng đoạn ≤ `cap` = f_max·T) và dữ liệu truyền
qua mỗi điểm cắt không vượt băng thông (d_k ≤ `link_cap` = B·T).

```
Cost(k+1, i) = E_on + g(S_i − S_k) + ε · d_k        (= +∞ nếu vi phạm cap hoặc link_cap)
DP(i, j)     = min_{j−1 ≤ k < i} { DP(k, j−1) + Cost(k+1, i) }
```

Hai mô hình năng lượng tính toán g(W):

| Mô hình | g(W) | Tham số JSON |
|---|---|---|
| Bậc ba (mặc định) | (κ/T²) · W³ | `kappa_over_T2` |
| Bậc hai (cho CHT) | a · W² + b · W | `"model": "quadratic"`, `quad_a`, `quad_b` |

Chi tiết ký hiệu và cách tính: [docs/model.md](docs/model.md). Thiết kế module: [docs/design.md](docs/design.md).

## Thuật toán đã cài

| `--algo` | Tệp | Thời gian | Bộ nhớ | Ghi chú |
|---|---|---|---|---|
| `baseline` | `src/baseline_dp.cpp` | O(M·N²) | O(M·N) | DP gốc, chỉ duyệt k trong cửa sổ `cap` (PR #14) |
| `dc` | `src/dc_opt.cpp` | O(M·N log N) | O(M·N) | D&C Optimization, dựa trên bất đẳng thức tứ giác; lọc trước điểm cắt bị cấm |
| `cht` | `src/cht_dp.cpp` | O(M·N) hoặc O(M·N log N) | O(N log N) | Convex Hull Trick, **chỉ mô hình bậc hai**: deque khi S_N ≤ cap, cây phân đoạn các bao lồi khi có cửa sổ cap |
| `bruteforce` | `src/brute_force.cpp` | O(2^N) | O(N) | Vét cạn, chỉ dùng làm chuẩn đối chiếu (N ≤ 18) |

Bốn heuristic so sánh (`src/heuristics.cpp`, chạy qua `edgedp_heur`): `greedy`, `equal`, `single`, `roundrobin`.

## Cấu trúc repo

```
include/edgedp/       types.hpp (Instance, Params, Solution), cost.hpp (hàm chi phí), solver.hpp, heuristics.hpp
src/                  baseline_dp, dc_opt, cht_dp, brute_force, heuristics, registry (danh sách solver),
                      json, io (đọc/ghi JSON), main (CLI edgedp), main_heuristics (CLI edgedp_heur)
bench/run_bench.cpp   harness đo thời gian và peak RSS, xuất CSV
tests/
  test_main.cpp       unit test C++ (make test)
  unittest2/          kiểm thử đối chiếu bằng pytest: biên, ví dụ chuẩn, ngẫu nhiên, hồi quy
  data/               example_n5.json (ví dụ chuẩn, bậc ba), example_n5_quadratic.json (bậc hai)
scripts/
  reproduce.sh        tái tạo toàn bộ thí nghiệm bằng một lệnh
  gen_data.py         sinh instance IoT tổng hợp và manifest.csv (gen_data_tv5.py là bản gốc giống hệt)
  make_quadratic_data.cpp  chuyển data/synthetic sang mô hình bậc hai (data/synthetic_quadratic)
  analyze_complexity.py    hồi quy log-log, đối chiếu thực nghiệm với lý thuyết (5.05)
  plot_results.py, plot_module_diagram.py, plot_style.py   vẽ hình cho báo cáo (6.01)
  prototype.py        prototype Python của DP gốc
data/synthetic/       300 instance bậc ba dùng trong báo cáo (+ manifest.csv)
data/synthetic_quadratic/  300 instance bậc hai tương ứng
results/              CSV kết quả benchmark và phân tích độ phức tạp
figures/              hình PDF (báo cáo) và PNG (slide)
docs/                 model.md, design.md, dp_optimizations.md, profiling.md, CONTRIBUTING.md, report/
.github/              CI (build + make test + pytest trên mỗi PR) và mẫu Pull Request
```

## Cài đặt

Cần:

- Trình biên dịch C++17 (g++ 9+ hoặc clang 10+) và `make`; hoặc CMake 3.14+.
- Python 3.8+. Riêng `gen_data.py` chỉ dùng thư viện chuẩn; phân tích, vẽ hình và pytest cần
  `numpy`, `pandas`, `matplotlib`, `pytest`.
- Linux hoặc macOS để chạy `run_bench` và `scripts/reproduce.sh` (trên Windows dùng WSL).
  Nên cài `/usr/bin/time` (`sudo apt install time`) để đo peak RSS chính xác; thiếu thì harness tự đọc `/proc`.

```bash
git clone https://github.com/Thuha69-hihi/edge-dp-schedule.git
cd edge-dp-schedule
python3 -m pip install -r requirements.txt     # Ubuntu 24.04: thêm --break-system-packages hoặc dùng venv
```

## Build và test

```bash
make                               # build/edgedp, build/edgedp_heur, build/run_bench, build/run_tests, build/brute_force_exec
make test                          # unit test C++
python3 -m pytest tests/unittest2 -q   # kiểm thử đối chiếu các solver với nhau và với vét cạn
make clean
```

Hoặc dùng CMake (Windows/Visual Studio, CLion):

```bash
cmake -S . -B build && cmake --build build && ctest --test-dir build
```

CI trên GitHub Actions chạy đúng ba bước `make`, `make test`, `pytest` cho mỗi Pull Request.

## Chạy solver

```bash
./build/edgedp --algo <baseline|dc|cht|bruteforce> --input <file.json>
```

Ví dụ chuẩn (mục 6 của tài liệu mô hình, mô hình bậc ba):

```bash
./build/edgedp --algo dc --input tests/data/example_n5.json
```

```json
{
  "feasible": true,
  "energy": 973,
  "nodes_used": 2,
  "cuts": [2],
  "segments": [
    {"from": 1, "to": 2, "work": 5, "cost": 378},
    {"from": 3, "to": 5, "work": 7, "cost": 595}
  ],
  "solver": "dc",
  "time_ms": 0.004837
}
```

`baseline`, `dc`, `bruteforce` đều cho `energy` = 973. `cht` trên file này dừng với mã thoát 3
(`solve_cht: chi ho tro model quadratic`), vì CHT chỉ áp dụng cho mô hình bậc hai. Cùng ví dụ ở mô hình
bậc hai (a = 1, b = 0) thì cả ba solver chính xác cho `energy` = 579:

```bash
./build/edgedp --algo cht --input tests/data/example_n5_quadratic.json
```

### Định dạng đầu vào

```json
{
  "n": 5, "m": 3,
  "w": [2, 3, 1, 4, 2],
  "d": [3, 4, 2, 6, 1],
  "params": {"kappa_over_T2": 1.0, "e_on": 250.0, "eps": 1.0, "cap": 8.0, "link_cap": 5.0}
}
```

| Trường | Ý nghĩa |
|---|---|
| `n`, `m` | số tác vụ N, số node tối đa M |
| `w` | N phần tử w₁..w_N, khối lượng tính toán mỗi tác vụ |
| `d` | N phần tử d₀..d_{N−1}; d_k là dữ liệu truyền nếu cắt sau tác vụ k |
| `params.kappa_over_T2` | κ/T² của mô hình bậc ba (bắt buộc có mặt, kể cả khi dùng bậc hai) |
| `params.e_on`, `params.eps` | năng lượng bật một node, năng lượng truyền trên một đơn vị dữ liệu |
| `params.cap`, `params.link_cap` | f_max·T và B·T |
| `params.model`, `quad_a`, `quad_b` | tuỳ chọn: `"quadratic"` để dùng g(W) = a·W² + b·W |
| `seed`, `source`, `meta` | tuỳ chọn, để truy vết nguồn gốc instance; solver bỏ qua |

### Định dạng đầu ra

`feasible`, `energy` (null nếu không khả thi), `nodes_used`, `cuts` (vị trí cắt k, tăng dần),
`segments` (từng đoạn: `from`, `to`, `work`, `cost`), `solver`, `time_ms` (thời gian giải, không gồm đọc file).
Mọi solver chính xác phải cho cùng `energy` (sai số tương đối ≤ 1e-9); `cuts` có thể khác khi có nhiều nghiệm tối ưu.

Mã thoát: 0 = thành công (kể cả khi không khả thi, lúc đó `feasible` = false), 2 = sai tham số dòng lệnh hoặc lỗi đọc đầu vào,
3 = solver không áp dụng được cho instance này (`cht` với mô hình bậc ba, `bruteforce` với N > 18).

## Sinh dữ liệu, heuristic và benchmark

### Sinh dữ liệu IoT tổng hợp (`scripts/gen_data.py`)

w_i ~ U{1..100}, d_i ~ U{1..50}; `cap` = α·S_N/M, `e_on` = β·(S_N/M)³, `link_cap` = phân vị 90% của d
(khoảng 10% vị trí không được cắt). Chi tiết trong docstring của script.

```bash
# Một instance
python3 scripts/gen_data.py --n 1000 --m 8 --seed 1 --alpha 4 --beta 10 --out data/x.json

# Lưới nhỏ để thử
python3 scripts/gen_data.py --grid --ns 1000 10000 --seeds 1 2 3 --out-dir data/small

# Lưới đầy đủ của báo cáo: N ∈ {10³, 10⁴, 10⁵} × M ∈ {4, 8, 16, 32} × 5 cặp (α, β) × 5 seed = 300 instance
python3 scripts/gen_data.py --grid --out-dir data/synthetic

# Bản bậc hai tương ứng (đọc data/synthetic, ghi data/synthetic_quadratic)
g++ -std=c++17 -O2 -I. scripts/make_quadratic_data.cpp src/json.cpp -o build/make_quadratic_data
./build/make_quadratic_data
```

### So sánh heuristic với DP (`edgedp_heur`)

```bash
./build/edgedp_heur --input tests/data/example_n5.json
```

```
DP baseline: energy=973.000000 nodes=2 time=0.00 ms
heuristic             energy  nodes  feas   v_cap  v_link        ms      saved%
greedy            973.000000      2   yes       0       0     0.002        0.00
equal            1051.000000      3    no       0       1     0.001        7.42
single           1981.000000      1    no       1       0     0.000       50.88
roundrobin       1108.000000      3    no       0       1     0.001       12.18
```

`saved%` = (E_heuristic − E_DP) / E_heuristic × 100; `feas = no` nghĩa là heuristic vi phạm `cap` hoặc `link_cap`.

### Benchmark thời gian và bộ nhớ (`run_bench`)

```bash
./build/run_bench --bin ./build/edgedp data/synthetic/manifest.csv \
    --solvers baseline,dc,cht --repeats 3 --timeout 600 \
    --out results/time_mem_cubic.csv --summary results/time_mem_summary_cubic.csv
```

- **Đầu vào:** file `.json`, thư mục, glob, hoặc `manifest.csv`. Khi đo cả lưới, hãy truyền `manifest.csv`:
  manifest liệt kê N tăng dần, và harness cần đo baseline N = 10⁴ trước để ngoại suy baseline N = 10⁵
  (100 × thời gian N = 10⁴, dòng có `status = timeout` và ghi chú `ngoai suy`), vì M·N² ≥ 10¹⁰ phép toán là quá lâu.
- **Đối chiếu nghiệm:** các solver chính xác được so `energy` với nhau (`match_ref` = `ok` hoặc `MISMATCH`);
  heuristic có thêm `energy_saved_pct`.
- `--max-n SOLVER=N` bỏ qua solver đó khi N lớn hơn ngưỡng; `--summary` gộp theo (solver, N, M, α, β).

## Tái tạo toàn bộ thí nghiệm bằng một lệnh

```bash
make reproduce          # = bash scripts/reproduce.sh
```

Script chạy lần lượt:

1. `make` build tất cả chương trình.
2. `make test` và `pytest tests/unittest2`.
3. Kiểm tra dữ liệu: dùng 300 instance đã commit trong `data/synthetic` và `data/synthetic_quadratic`
   (sinh lại nếu thiếu).
4. Benchmark mô hình bậc ba: `baseline`, `dc` (`cht` ghi `not_impl` là đúng), 3 lần lặp
   → `results/time_mem_cubic.csv`, `results/time_mem_summary_cubic.csv`.
5. Benchmark mô hình bậc hai: `baseline`, `dc`, `cht` → `results/time_mem.csv`, `results/time_mem_summary.csv`.
   Script dừng với lỗi nếu có dòng `MISMATCH`.
6. `analyze_complexity.py` → `results/complexity_{slopes,summary}.csv`, `figures/loglog_time_vs_n.*`;
   `plot_results.py` và `plot_module_diagram.py` → các hình `figures/fig_*`.

Thời gian chạy đầy đủ khoảng 20–30 phút trên máy 2 vCPU (phần lớn là baseline N = 10⁴).

| Tuỳ chọn | Tác dụng |
|---|---|
| `--quick` (hoặc `make reproduce-quick`) | lưới nhỏ (N = 10³, 10⁴; M = 4, 8; seed 1), khoảng 1–2 phút, ghi vào `build/quick/`, không ghi đè `results/` và `figures/` |
| `--regen-data` | sinh lại dữ liệu bằng `gen_data.py` thay vì dùng bản đã commit |
| `--skip-tests` | bỏ qua bước 2 |
| `--repeats K` | số lần lặp mỗi solver (mặc định 3) |

Lưu ý khi so số liệu:

- Năng lượng tối ưu (`energy`) tái tạo chính xác. Thời gian và bộ nhớ phụ thuộc máy; số trong báo cáo đo trên
  Linux, Xeon 2,8 GHz, 2 vCPU, g++ 13 `-O2`. Kết luận về bậc tăng (độ dốc log-log) không phụ thuộc máy.
- `--regen-data` trên máy khác có thể cho `e_on`, `eps` lệch ở chữ số thập phân cuối do làm tròn số thực,
  nên mặc định dùng bộ dữ liệu đã commit để khớp đúng báo cáo.

## Kết quả chính

Độ dốc log-log của thời gian theo N (trung bình trên 100 cấu hình mỗi solver, `results/complexity_summary.csv`).
Với N ∈ [10³, 10⁵], bậc N log N có độ dốc lý thuyết khoảng 1,11.

| Mô hình | Solver | Độ dốc đo được (KTC 95%) | Lý thuyết |
|---|---|---|---|
| Bậc ba | baseline | 1,96 (1,94–1,98) | 2 |
| Bậc ba | dc | 1,10 (1,09–1,11) | ≈ 1,11 |
| Bậc hai | baseline | 1,94 (1,92–1,96) | 2 |
| Bậc hai | dc | 1,09 (1,07–1,10) | ≈ 1,11 |
| Bậc hai | cht (deque) | 1,03 (1,00–1,06) | 1 |
| Bậc hai | cht (cây phân đoạn) | 1,08 (1,07–1,09) | ≈ 1,11 |

Thời gian trung bình (ms), mô hình bậc ba, trung bình trên mọi M:

| N | baseline | dc |
|---|---|---|
| 10³ | 17,0 | 1,1 |
| 10⁴ | 1 497 | 14,4 |
| 10⁵ | ≈ 151 900 (ngoại suy) | 170 |

Phân tích đầy đủ (theo M, speedup, năng lượng tiết kiệm so với heuristic) nằm ở Chương 6 của báo cáo;
mục đối chiếu lý thuyết có bản LaTeX ở [docs/report/kiem_chung_thuc_nghiem.tex](docs/report/kiem_chung_thuc_nghiem.tex).

## Đóng góp

Xem [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md): không push thẳng `main`, mọi thay đổi qua Pull Request có review,
`make test` và pytest phải xanh trước khi mở PR. Thêm một solver mới: cài `Solution solve_xxx(const Instance&)` trong `src/`,
khai báo trong `include/edgedp/solver.hpp` và đăng ký trong `src/registry.cpp`; test đối chiếu sẽ tự chạy với solver đó.
