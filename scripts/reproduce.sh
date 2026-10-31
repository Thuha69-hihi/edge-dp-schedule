#!/usr/bin/env bash
# Tái tạo toàn bộ thí nghiệm của báo cáo bằng một lệnh (việc 6.05).
#
#   bash scripts/reproduce.sh            # đầy đủ: build, test, benchmark 2 x 300 instance, phân tích, vẽ hình
#   bash scripts/reproduce.sh --quick    # thử nhanh (~1 phút) trên lưới nhỏ, ghi vào build/quick/, không đụng results/
#
# Tuỳ chọn:
#   --quick         lưới nhỏ (N = 1000, 10000; M = 4, 8; seed 1), chỉ để kiểm tra pipeline chạy được
#   --regen-data    sinh lại data/synthetic bằng gen_data.py thay vì dùng bộ dữ liệu đã commit
#                   (khác máy có thể lệch ở chữ số cuối của e_on/eps, nên mặc định dùng bản đã commit)
#   --skip-tests    bỏ qua make test và pytest
#   --repeats K     số lần lặp mỗi solver (mặc định 3, như báo cáo)
#
# Cần: Linux hoặc macOS, g++ hỗ trợ C++17, make, Python 3 với numpy, pandas, matplotlib, pytest
# (pip install -r requirements.txt). Nên có /usr/bin/time để đo peak RSS.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

QUICK=0; REGEN=0; TESTS=1; REPEATS=3
while [[ $# -gt 0 ]]; do
  case "$1" in
    --quick) QUICK=1 ;;
    --regen-data) REGEN=1 ;;
    --skip-tests) TESTS=0 ;;
    --repeats) REPEATS="$2"; shift ;;
    -h|--help) sed -n '2,17p' "$0"; exit 0 ;;
    *) echo "Tham số không hợp lệ: $1 (xem --help)" >&2; exit 2 ;;
  esac
  shift
done

PY="${PYTHON:-python3}"
step() { printf '\n==> [%s] %s\n' "$(date +%H:%M:%S)" "$*"; }

step "1/6 Build (make)"
make -s all

if [[ $TESTS -eq 1 ]]; then
  step "2/6 Unit test C++ và kiểm thử đối chiếu (pytest)"
  make -s test
  "$PY" -m pytest tests/unittest2 -q
else
  step "2/6 Bỏ qua test (--skip-tests)"
fi

# Bộ sinh dữ liệu bậc hai: đọc <cwd>/data/synthetic, ghi <cwd>/data/synthetic_quadratic.
g++ -std=c++17 -O2 -I. scripts/make_quadratic_data.cpp src/json.cpp -o build/make_quadratic_data

if [[ $QUICK -eq 1 ]]; then
  OUT="build/quick"
  DATA="$OUT/data/synthetic"
  QDATA="$OUT/data/synthetic_quadratic"
  RES="$OUT/results"
  rm -rf "$OUT"; mkdir -p "$RES"
  step "3/6 Sinh lưới nhỏ vào $OUT/data"
  "$PY" scripts/gen_data.py --grid --ns 1000 10000 --ms 4 8 --seeds 1 --out-dir "$DATA" > /dev/null
  (cd "$OUT" && "$ROOT/build/make_quadratic_data" > /dev/null)
else
  DATA="data/synthetic"
  QDATA="data/synthetic_quadratic"
  RES="results"
  step "3/6 Dữ liệu: 300 instance bậc ba + 300 instance bậc hai"
  if [[ $REGEN -eq 1 || ! -f "$DATA/manifest.csv" ]]; then
    "$PY" scripts/gen_data.py --grid --out-dir "$DATA" > /dev/null
  fi
  if [[ $REGEN -eq 1 || ! -f "$QDATA/manifest.csv" ]]; then
    ./build/make_quadratic_data > /dev/null
  fi
fi
echo "    $DATA: $(($(wc -l < "$DATA/manifest.csv") - 1)) instance"
echo "    $QDATA: $(($(wc -l < "$QDATA/manifest.csv") - 1)) instance"

# Phải truyền manifest.csv (không phải thư mục): manifest liệt kê N tăng dần, run_bench cần đo baseline
# N = 10^4 trước để ngoại suy baseline N = 10^5 (100 x thời gian N = 10^4, xem bench/run_bench.cpp).
BENCH=(./build/run_bench --bin ./build/edgedp --solvers baseline,dc,cht --repeats "$REPEATS" --timeout 600)

step "4/6 Benchmark mô hình bậc ba (baseline, dc; cht báo not_impl là đúng)"
"${BENCH[@]}" --out "$RES/time_mem_cubic.csv" --summary "$RES/time_mem_summary_cubic.csv" "$DATA/manifest.csv"

step "5/6 Benchmark mô hình bậc hai (baseline, dc, cht)"
"${BENCH[@]}" --out "$RES/time_mem.csv" --summary "$RES/time_mem_summary.csv" "$QDATA/manifest.csv"

MISMATCH=$(cat "$RES/time_mem.csv" "$RES/time_mem_cubic.csv" | grep -c ',MISMATCH,' || true)
if [[ "$MISMATCH" -ne 0 ]]; then
  echo "LỖI: $MISMATCH dòng có năng lượng lệch giữa các solver chính xác (match_ref = MISMATCH)" >&2
  exit 1
fi
echo "    Năng lượng khớp giữa các solver chính xác (không có MISMATCH)."

if [[ $QUICK -eq 1 ]]; then
  step "6/6 Xong chế độ --quick. Kết quả ở $RES/ (results/ và figures/ không bị thay đổi)."
  exit 0
fi

step "6/6 Phân tích độ phức tạp (5.05) và vẽ hình (6.01)"
"$PY" scripts/analyze_complexity.py
"$PY" scripts/plot_results.py --results "$RES" --data "$DATA"
"$PY" scripts/plot_module_diagram.py

step "Hoàn tất. Kết quả: results/*.csv, hình: figures/*.pdf|png"
