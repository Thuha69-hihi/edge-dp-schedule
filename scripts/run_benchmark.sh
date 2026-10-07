#!/bin/sh
# Chạy thử so sánh thời gian giữa các thuật toán trên nhiều cỡ N (bản tạm).
# Dùng: sh scripts/run_benchmark.sh baseline [cht dc ...]
# TV5 sẽ mở rộng (lặp nhiều lần, đo bộ nhớ, xuất CSV).
set -e
mkdir -p data/generated
make >/dev/null
for n in 1000 2000 4000; do
  python3 scripts/gen_data.py --n "$n" --m 8 --seed 1 > "data/generated/n$n.json"
  for algo in "$@"; do
    printf "N=%s " "$n"
    ./build/edgedp --algo "$algo" --input "data/generated/n$n.json" | tr -d '\n' | sed 's/  */ /g'
    echo
  done
done
