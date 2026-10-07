#!/bin/sh
# Chạy thử so sánh thời gian giữa các thuật toán trên nhiều cỡ N.
# Dùng: sh scripts/run_benchmark.sh baseline cht
# TV5 sẽ mở rộng (lặp nhiều lần, đo bộ nhớ, xuất CSV).
set -e
mkdir -p data/generated
make >/dev/null
for n in 1000 2000 4000; do
  python3 scripts/gen_data.py --n "$n" --m 3 --seed 1 > "data/generated/n$n.txt"
  for algo in "$@"; do
    ./build/edgedp --algo "$algo" --input "data/generated/n$n.txt" || true
  done
done
