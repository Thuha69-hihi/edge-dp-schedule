#!/usr/bin/env python3
"""Sinh dữ liệu mô phỏng theo định dạng input của edgedp.

Ví dụ:
    python3 scripts/gen_data.py --n 1000 --m 3 --seed 1 > data/generated/n1000.txt
(Thư mục data/generated/ đã nằm trong .gitignore; chỉ commit file nhỏ hoặc script sinh.)
TV5 sẽ mở rộng để sinh theo phân phối giống IoT / Google Cluster Data.
"""
import argparse
import random


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, required=True, help="số tác vụ")
    ap.add_argument("--m", type=int, default=3, help="số node")
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--max-work", type=int, default=20)
    a = ap.parse_args()

    rng = random.Random(a.seed)
    print(a.n, a.m)
    print(" ".join(str(rng.randint(1, a.max_work)) for _ in range(a.n)))
    for _ in range(a.m):
        print(rng.randint(1, 5), rng.randint(0, 200))


if __name__ == "__main__":
    main()
