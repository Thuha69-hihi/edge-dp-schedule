#!/usr/bin/env python3
"""scripts/gen_data.py - Bộ sinh dữ liệu IoT tổng hợp (việc 2.06, TV5 Thành Công).

Sinh instance JSON đúng định dạng mục 8 của "Phát biểu hình thức bài toán" (1.05) và
tham số theo bảng "Tham số sinh dữ liệu" ở mục 9 (quyết định tuần 1):

    w_i   ~ U{1..100}  (10^6 chu kỳ)          d_i ~ U{1..50} (KB), d_0..d_{N-1}
    W_bar = S_N / M
    cap       = alpha * W_bar     (= f_max*T)
    e_on      = beta  * W_bar^3
    eps       : eps * mean(d) = 0.05 * e_on
    link_cap  = phân vị 90% của d (khoảng 10% vị trí không được cắt)
    kappa_over_T2 = 1 (chuẩn hóa)

Đảm bảo có nghiệm: nếu cap = alpha*W_bar quá chặt (nhỏ hơn max w_i, hoặc số đoạn tối
thiểu hợp lệ > M) thì nhân cap lên 1.05 lần cho tới khi khả thi. Khi đó meta.alpha_eff
> meta.alpha và meta.cap_adjusted = true (hiếm, chủ yếu khi N nhỏ).

Tái lập: cùng (n, m, seed, alpha, beta, model) cho ra file giống hệt từng byte. w và d dùng
hai luồng ngẫu nhiên riêng theo seed, nên w của N=10^3 là tiền tố của w của N=10^4 (cùng seed).

Ví dụ:
    python scripts/gen_data.py --n 1000 --m 8 --seed 1 --alpha 4 --beta 10 --out data/x.json
    python scripts/gen_data.py --grid --out-dir data/synthetic          # lưới của việc 5.01
    python scripts/gen_data.py --grid --ns 1000 10000 --seeds 1 2 3 --out-dir data/small

Chỉ dùng thư viện chuẩn của Python (>= 3.8).
"""
import argparse
import bisect
import csv
import json
import math
import os
import random
import sys

W_RANGE = (1, 100)
D_RANGE = (1, 50)
LINK_PERCENTILE = 0.90
TX_FRACTION = 0.05
CAP_GROWTH = 1.05


def min_segments(w, d, cap, link_cap):
    """Số đoạn liên tiếp tối thiểu thỏa mọi ràng buộc, hoặc None nếu không có cách.

    Ràng buộc: khối lượng mỗi đoạn <= cap; cắt sau tác vụ c (1 <= c <= N-1) chỉ được khi
    d[c] <= link_cap. Tham lam "đi xa nhất" là tối ưu cho số đoạn.
    """
    n = len(w)
    S = [0] * (n + 1)
    for i, x in enumerate(w):
        S[i + 1] = S[i] + x
    k = 0
    count = 0
    while k < n:
        i = bisect.bisect_right(S, S[k] + cap) - 1
        while k < i < n and d[i] > link_cap:
            i -= 1
        if i <= k:
            return None
        count += 1
        k = i
    return count


def gen_instance(n, m, seed, alpha, beta, model="cubic"):
    rw = random.Random("w-%d" % seed)
    rd = random.Random("d-%d" % seed)
    w = [rw.randint(*W_RANGE) for _ in range(n)]
    d = [rd.randint(*D_RANGE) for _ in range(n)]

    w_bar = sum(w) / m
    d_bar = sum(d) / n
    link_cap = float(sorted(d)[max(0, math.ceil(LINK_PERCENTILE * n) - 1)])
    e_on = beta * w_bar ** 3
    eps = TX_FRACTION * e_on / d_bar

    cap = max(alpha * w_bar, float(max(w)))
    while True:
        c = min_segments(w, d, cap, link_cap)
        if c is not None and c <= m:
            break
        cap *= CAP_GROWTH
    alpha_eff = cap / w_bar

    params = {
        "kappa_over_T2": 1.0,
        "e_on": e_on,
        "eps": eps,
        "cap": cap,
        "link_cap": link_cap,
        "model": model,
    }
    if model == "quadratic":
        # Biến thể bậc hai E_exec = a*W^2 + b*W (cho CHT). Chọn a để E(W_bar) bằng bậc ba.
        params["a"] = params["kappa_over_T2"] * w_bar
        params["b"] = 0.0
    return {
        "n": n,
        "m": m,
        "w": w,
        "d": d,
        "params": params,
        "seed": seed,
        "source": "synthetic",
        "meta": {
            "alpha": alpha,
            "beta": beta,
            "alpha_eff": alpha_eff,
            "cap_adjusted": alpha_eff > alpha * (1 + 1e-12),
            "w_bar": w_bar,
        },
    }


def write_json(inst, path, indent=None):
    d = os.path.dirname(path)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(inst, f, separators=(",", ":") if indent is None else None,
                  indent=indent, allow_nan=False)
        f.write("\n")


def grid_name(n, m, alpha, beta, seed, model):
    s = "syn_n%d_m%d_a%g_b%g_s%d" % (n, m, alpha, beta, seed)
    return s + ("_quad" if model == "quadratic" else "") + ".json"


def run_grid(args):
    combos = [(a, b) for a in args.alphas for b in args.betas]
    if not args.all_combos:
        # Mục 9: beta = 100 chỉ có ý nghĩa với alpha lớn (cần j >= M/alpha cho nghiệm ~0.27M node).
        combos = [(a, b) for a, b in combos if not (b >= 100 and a < 3)]
    rows = []
    total = len(args.ns) * len(args.ms) * len(args.seeds) * len(combos)
    done = 0
    for n in args.ns:
        for m in args.ms:
            for seed in args.seeds:
                for a, b in combos:
                    name = grid_name(n, m, a, b, seed, args.model)
                    inst = gen_instance(n, m, seed, a, b, args.model)
                    write_json(inst, os.path.join(args.out_dir, name))
                    rows.append({"file": name, "n": n, "m": m, "alpha": a, "beta": b,
                                 "alpha_eff": inst["meta"]["alpha_eff"], "seed": seed,
                                 "source": "synthetic", "model": args.model})
                    done += 1
                    if done % 20 == 0 or done == total:
                        print("[%d/%d] %s" % (done, total, name), file=sys.stderr)
    manifest = os.path.join(args.out_dir, "manifest.csv")
    with open(manifest, "w", newline="", encoding="utf-8") as f:
        wr = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        wr.writeheader()
        wr.writerows(rows)
    print("Đã ghi %d file + %s" % (len(rows), manifest), file=sys.stderr)


def main(argv=None):
    p = argparse.ArgumentParser(description="Sinh instance IoT tổng hợp (việc 2.06).")
    p.add_argument("--model", choices=["cubic", "quadratic"], default="cubic")
    p.add_argument("--grid", action="store_true", help="sinh cả lưới thí nghiệm (việc 5.01)")
    p.add_argument("--out-dir", default="data/synthetic")
    p.add_argument("--ns", type=int, nargs="+", default=[1000, 10000, 100000])
    p.add_argument("--ms", type=int, nargs="+", default=[4, 8, 16, 32])
    p.add_argument("--seeds", type=int, nargs="+", default=[1, 2, 3, 4, 5])
    p.add_argument("--alphas", type=float, nargs="+", default=[1.2, 4.0])
    p.add_argument("--betas", type=float, nargs="+", default=[1.0, 10.0, 100.0])
    p.add_argument("--all-combos", action="store_true", help="giữ cả cặp (alpha nhỏ, beta=100)")
    p.add_argument("--n", type=int, default=1000)
    p.add_argument("--m", type=int, default=8)
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--alpha", type=float, default=4.0)
    p.add_argument("--beta", type=float, default=10.0)
    p.add_argument("--out", default="-", help="đường dẫn file JSON, '-' = stdout")
    p.add_argument("--indent", type=int, default=None)
    args = p.parse_args(argv)

    if args.grid:
        run_grid(args)
        return
    inst = gen_instance(args.n, args.m, args.seed, args.alpha, args.beta, args.model)
    if args.out == "-":
        json.dump(inst, sys.stdout, separators=(",", ":") if args.indent is None else None,
                  indent=args.indent, allow_nan=False)
        sys.stdout.write("\n")
    else:
        write_json(inst, args.out, args.indent)


if __name__ == "__main__":
    main()
