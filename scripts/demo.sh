#!/usr/bin/env bash
# Demo 7.05: so thoi gian cac solver. Chay tu thu muc goc repo:  bash scripts/demo.sh
#   FULL=1 bash scripts/demo.sh   -> chay ca baseline o N=1e5 (khoang 1 phut)
set -u
BIN=build/edgedp
CUBIC_SMALL=${CUBIC_SMALL:-data/synthetic/syn_n10000_m16_a1.2_b10_s1.json}
CUBIC_BIG=${CUBIC_BIG:-data/synthetic/syn_n100000_m16_a1.2_b10_s1.json}
QUAD_BIG=${QUAD_BIG:-data/synthetic_quadratic/syn_n100000_m16_a1.2_b10_s1.json}

[ -x "$BIN" ] || make build/edgedp || { echo "Build that bai"; exit 1; }

run() {  # run <algo> <file>  -> in 1 dong: solver, energy, time_ms
  "$BIN" --algo "$1" --input "$2" | python3 -c "
import sys, json
d = json.load(sys.stdin)
print('  %-9s feasible=%-5s  energy=%-22s  time=%10.1f ms' % (d['solver'], d['feasible'], d['energy'], d['time_ms']))"
}

echo "== Buoc 1: N=10^4, M=16, mo hinh bac ba: baseline va D&C cho CUNG nang luong"
run baseline "$CUBIC_SMALL"
run dc       "$CUBIC_SMALL"

echo
echo "== Buoc 2: N=10^5, M=16, mo hinh bac ba: D&C"
run dc "$CUBIC_BIG"
if [ "${FULL:-0}" = "1" ]; then
  echo "   (baseline o N=10^5, khoang 1 phut...)"
  run baseline "$CUBIC_BIG"
else
  echo "   baseline o N=10^5 mat khoang 1 phut (do truoc: 58 s); chay FULL=1 de xem truc tiep"
fi

echo
echo "== Buoc 3: N=10^5, M=16, mo hinh bac hai: D&C va CHT cho cung nang luong"
if [ -f "$QUAD_BIG" ]; then
  run dc  "$QUAD_BIG"
  run cht "$QUAD_BIG"
else
  echo "   Khong thay $QUAD_BIG (can merge PR #12 de co data/synthetic_quadratic)"
fi
