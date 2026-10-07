#!/usr/bin/env python3
"""Sinh dữ liệu mô phỏng theo định dạng JSON ở mục 8 của tài liệu mô hình.

Ví dụ:
    python3 scripts/gen_data.py --n 1000 --m 8 --seed 1 > data/generated/n1000.json
(Thư mục data/generated/ nằm trong .gitignore; chỉ commit file nhỏ hoặc script sinh.)

BẢN TẠM của TV3 để chạy thử baseline. TV5 sẽ thay bằng bộ sinh chính thức (việc 2.06):
khoảng tham số theo khảo sát 1.06, có cả trường hợp dùng ít và dùng hết M node.
Mặc định cap = 1.5 * (tổng w / m) nên luôn đủ chỗ cho m node và thường có nghiệm.
"""
import argparse
import json
import math
import random


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, required=True, help="số tác vụ N")
    ap.add_argument("--m", type=int, default=8, help="số node tối đa M")
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--max-w", type=int, default=20)
    ap.add_argument("--max-d", type=int, default=20)
    ap.add_argument("--e-on", type=float, default=250.0)
    ap.add_argument("--eps", type=float, default=1.0)
    ap.add_argument("--kappa-over-t2", type=float, default=1.0)
    ap.add_argument("--cap-factor", type=float, default=1.5,
                    help="cap = cap_factor * tổng_w / m (nhưng không nhỏ hơn max w)")
    a = ap.parse_args()

    rng = random.Random(a.seed)
    w = [rng.randint(1, a.max_w) for _ in range(a.n)]
    d = [rng.randint(0, a.max_d) for _ in range(a.n)]
    cap = max(max(w), math.ceil(a.cap_factor * sum(w) / a.m))
    link_cap = max(d)  # mặc định không chặn chỗ cắt nào

    print(json.dumps({
        "n": a.n,
        "m": a.m,
        "w": w,
        "d": d,
        "params": {
            "kappa_over_T2": a.kappa_over_t2,
            "e_on": a.e_on,
            "eps": a.eps,
            "cap": float(cap),
            "link_cap": float(link_cap),
        },
        "seed": a.seed,
        "source": "synthetic",
    }))


if __name__ == "__main__":
    main()
