# Edge-DP-Scheduler

Nhóm 2, môn Phân tích thiết kế thuật toán (NEU).
Đề tài: **Quy hoạch động cải tiến cho lập lịch tác vụ tiết kiệm năng lượng trên Edge/IoT**
(Convex Hull Trick, Divide and Conquer Optimization).

> Trạng thái: khung repo (tuần 1). Mô hình hiện tại là bản tạm, xem [docs/model.md](docs/model.md).

## Cấu trúc

```
include/edgedp/   kiểu dữ liệu và giao diện chung (types.hpp, solver.hpp)
src/              baseline_dp, cht_dp, dc_opt, brute_force, io, main (CLI)
tests/            unit test (không cần thư viện ngoài)
scripts/          sinh dữ liệu, chạy benchmark
data/             input mẫu
docs/             mô hình bài toán, quy ước làm việc
.github/          CI và mẫu Pull Request
```

## Yêu cầu
- Trình biên dịch hỗ trợ C++17 (g++ 9+, clang, MSVC)
- `make` (Linux/macOS/MSYS2) hoặc CMake 3.14+
- Python 3 (chỉ để sinh dữ liệu)

## Build và test

```bash
make          # build ./build/edgedp và ./build/run_tests
make test     # chạy unit test
make clean
```

Hoặc dùng CMake:

```bash
cmake -S . -B build && cmake --build build && ctest --test-dir build
```

## Chạy

```bash
./build/edgedp --algo baseline --input data/sample.txt --show
```

Kết quả mong đợi với `data/sample.txt`: `energy=38`, 2 nhóm `[0,2)` và `[2,3)`.

Thuật toán: `baseline`, `cht`, `dc`, `bruteforce` (cái nào chưa cài sẽ báo "chua cai dat").

### Định dạng input

```
N M
w[0] w[1] ... w[N-1]
alpha_0 setup_0
...
alpha_{M-1} setup_{M-1}
```

### Sinh dữ liệu và benchmark thử

```bash
python3 scripts/gen_data.py --n 1000 --m 3 --seed 1 > data/generated/n1000.txt
sh scripts/run_benchmark.sh baseline
```

## Tiến độ cài đặt

| Thành phần | Người phụ trách | Trạng thái |
|---|---|---|
| Khung repo, giao diện chung, CLI, test | TV3 | xong |
| Baseline DP O(M·N²) | TV3 | bản v0 theo mô hình tạm |
| Convex Hull Trick | TV3 | chưa làm |
| Brute force, D&C Optimization | TV4 | chưa làm |
| Dữ liệu và benchmark | TV5 | chưa làm |

## Đóng góp
Xem [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md): không push thẳng `main`, mọi thay đổi qua Pull Request có review.
