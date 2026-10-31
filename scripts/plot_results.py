#!/usr/bin/env python3
"""Vẽ đồ thị kết quả thực nghiệm cho Chương 6 (việc 6.01).

Đầu vào:
    results/time_mem_cubic.csv   benchmark mô hình bậc ba (baseline, dc)
    results/time_mem.csv         benchmark mô hình bậc hai (baseline, dc, cht)
    data/synthetic/*.json        instance bậc ba, để tính năng lượng của 4 heuristic
Đầu ra (figures/, PDF + PNG):
    fig_time_vs_n       thời gian chạy theo N
    fig_speedup         Speedup = T_baseline / T_solver theo M
    fig_memory          bộ nhớ đỉnh (peak RSS) theo N
    fig_energy_saved    Energy Saved % của DP so với 4 heuristic theo M

Điểm = trung bình trên 25 instance (5 seed × 5 cấu hình phân phối α, β). Error bar = độ lệch chuẩn
giữa các seed, gộp (pooled) trên 5 cấu hình: sqrt(trung bình phương sai từng cấu hình), nên chỉ phản ánh
dao động do seed, không lẫn khác biệt giữa các cấu hình. Đồ thị theo N cố định M = M_FIXED.

Dùng:
    python3 scripts/plot_results.py [--results results] [--data data/synthetic]
"""
from __future__ import annotations

import argparse
import csv
import glob
import json
import math
import os
import statistics
import sys
from collections import defaultdict

import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import plot_style as ps  # noqa: E402

NS = [1000, 10000, 100000]
MS = [4, 8, 16, 32]
M_FIXED = 16
MODELS = [("cubic", "time_mem_cubic.csv", ["baseline", "dc"]),
          ("quadratic", "time_mem.csv", ["baseline", "dc", "cht"])]
MODEL_TITLE = {"cubic": "Mô hình bậc ba", "quadratic": "Mô hình bậc hai"}


def load(path):
    """Các lần chạy thành công: list dict với n, m, file, solver, time_ms, peak_rss_mb, energy."""
    out = []
    with open(path, encoding="utf-8") as f:
        for r in csv.DictReader(f):
            if r["status"] != "ok":
                continue
            out.append({"file": r["file"], "solver": r["solver"], "n": int(r["n"]), "m": int(r["m"]),
                        "cfg": (r["alpha"], r["beta"]),
                        "time_ms": float(r["time_ms"]), "peak_rss_mb": float(r["peak_rss_mb"]),
                        "energy": float(r["energy"])})
    return out


def mean_std(pairs):
    """pairs = [(cfg, giá trị)]. Trả (trung bình chung, độ lệch chuẩn giữa seed gộp theo cfg)."""
    if not pairs:
        return math.nan, math.nan
    by = defaultdict(list)
    for cfg, v in pairs:
        by[cfg].append(v)
    var = [statistics.variance(v) for v in by.values() if len(v) > 1]
    return statistics.fmean(v for _, v in pairs), math.sqrt(statistics.fmean(var)) if var else 0.0


def errorbar(ax, x, ys, key, label=None, **kw):
    mu = [mean_std(v)[0] for v in ys]
    sd = [mean_std(v)[1] for v in ys]
    st = ps.solver(key)
    st.update(kw)
    ax.errorbar(x, mu, yerr=sd, capsize=2, elinewidth=0.6, label=label or ps.LABELS[key], **st)
    return mu, sd


# ---------------------------------------------------------------- heuristics (port của src/heuristics.cpp)

def heuristic_energies(inst):
    """Năng lượng 4 heuristic theo đúng src/heuristics.cpp (mô hình bậc ba). Trả {tên: (energy, feasible)}."""
    n, m, w, d, p = inst["n"], inst["m"], inst["w"], inst["d"], inst["params"]
    kap, e_on, eps, cap, link = p["kappa_over_T2"], p["e_on"], p["eps"], p["cap"], p["link_cap"]
    S = np.concatenate([[0], np.cumsum(np.asarray(w, dtype=np.int64))])

    def contiguous(ends):
        e, k, vc, vl = 0.0, 0, 0, 0
        for c in ends:
            W = float(S[c] - S[k])
            e += e_on + kap * W ** 3 + eps * d[k]
            vc += W > cap
            vl += k >= 1 and d[k] > link
            k = c
        return e, vc == 0 and vl == 0 and len(ends) <= m

    out = {}
    # greedy first-fit, cắt ở vị trí hợp lệ xa nhất
    ends, k = [], 0
    while k < n:
        if len(ends) == m - 1:
            ends.append(n)
            break
        imax = int(np.searchsorted(S, S[k] + int(cap), side="right")) - 1
        if imax <= k:
            imax = k + 1
        i = imax
        while k < i < n and d[i] > link:
            i -= 1
        if i == k:
            i = imax
        ends.append(i)
        k = i
    out["greedy"] = contiguous(ends)
    j = min(m, n)
    out["equal"] = contiguous([s * n // j for s in range(1, j + 1)])
    out["single"] = contiguous([n])
    load_ = np.bincount(np.arange(n) % m, weights=np.asarray(w, dtype=float), minlength=m)
    e = sum(e_on + kap * L ** 3 for L in load_ if L > 0) + eps * d[0]
    feas = all(L <= cap for L in load_)
    if m >= 2:
        e += eps * sum(d[1:n])
        feas = feas and all(x <= link for x in d[1:n])
    out["roundrobin"] = (e, feas)
    return out


def _selfcheck():
    # Ví dụ Mục 3.6 (tests/data/example_n5.json): một node có W = 12 -> 250 + 12^3 + d_0 = 1981.
    inst = {"n": 5, "m": 3, "w": [2, 3, 1, 4, 2], "d": [3, 4, 2, 6, 1],
            "params": {"kappa_over_T2": 1.0, "e_on": 250.0, "eps": 1.0, "cap": 8.0, "link_cap": 5.0}}
    h = heuristic_energies(inst)
    assert h["single"] == (1981.0, False), h["single"]
    # chia đều 3 đoạn: (0,1],(1,3],(3,5] -> 250+8+3 + 250+64+4 + 250+216+6 (d_3 = 6 > L nên vi phạm)
    assert h["equal"] == (1051.0, False), h["equal"]


# ---------------------------------------------------------------- figures

def fig_time(data):
    fig, axes = plt.subplots(1, 2, figsize=ps.WIDE, sharey=True)
    for ax, (model, _, solvers) in zip(axes, MODELS):
        rows = data[model]
        for s in solvers:
            xs = [n for n in NS if any(r["solver"] == s and r["n"] == n for r in rows)]
            errorbar(ax, xs, [[(r["cfg"], r["time_ms"]) for r in rows
                               if r["solver"] == s and r["n"] == n and r["m"] == M_FIXED] for n in xs], s)
        ax.set(xscale="log", yscale="log", xlabel="Số tác vụ $N$", title=MODEL_TITLE[model])
    axes[0].set_ylabel("Thời gian chạy (ms)")
    axes[1].legend(loc="upper left")
    ps.save(fig, "fig_time_vs_n")


def fig_speedup(data):
    fig, axes = plt.subplots(1, 2, figsize=ps.WIDE, sharey=True)
    for ax, (model, _, solvers) in zip(axes, MODELS):
        base = {r["file"]: r["time_ms"] for r in data[model] if r["solver"] == "baseline"}
        for s in solvers[1:]:
            for n, ls in ((10000, "-"), (1000, "--")):
                ys = [[(r["cfg"], base[r["file"]] / r["time_ms"]) for r in data[model]
                       if r["solver"] == s and r["n"] == n and r["m"] == m and r["file"] in base] for m in MS]
                errorbar(ax, MS, ys, s, label=f"{ps.LABELS[s].split(' ')[0]}, $N=10^{{{int(math.log10(n))}}}$",
                         linestyle=ls)
        ax.set(xscale="log", yscale="log", xlabel="Số node tối đa $M$", title=MODEL_TITLE[model])
        ax.set_xticks(MS, [str(m) for m in MS])
        ax.xaxis.set_minor_locator(mticker.NullLocator())
        ax.yaxis.set_major_locator(mticker.LogLocator(subs=(1, 2, 5)))
        ax.yaxis.set_major_formatter(mticker.ScalarFormatter())
        ax.yaxis.set_minor_locator(mticker.NullLocator())
    axes[0].set_ylabel("Speedup so với DP gốc (lần)")
    # Legend chung phía trên để không che đường CHT ở M = 4.
    fig.legend(*axes[1].get_legend_handles_labels(), loc="lower center", ncol=4, bbox_to_anchor=(0.5, 0.98))
    ps.save(fig, "fig_speedup")


def fig_memory(data):
    fig, axes = plt.subplots(1, 2, figsize=ps.WIDE, sharey=True)
    for ax, (model, _, solvers) in zip(axes, MODELS):
        rows = data[model]
        for s in solvers:
            xs = [n for n in NS if any(r["solver"] == s and r["n"] == n for r in rows)]
            errorbar(ax, xs, [[(r["cfg"], r["peak_rss_mb"]) for r in rows
                               if r["solver"] == s and r["n"] == n and r["m"] == M_FIXED] for n in xs], s)
        ax.set(xscale="log", xlabel="Số tác vụ $N$", title=MODEL_TITLE[model])
    axes[0].set_ylabel("Bộ nhớ đỉnh (MB)")
    axes[1].legend(loc="upper left")
    ps.save(fig, "fig_memory")


def fig_energy_saved(data, data_dir):
    opt = {r["file"]: (r["energy"], (r["n"],) + r["cfg"]) for r in data["cubic"] if r["solver"] == "dc"}
    saved = defaultdict(list)       # (heuristic, m) -> [%]
    infeasible = defaultdict(int)   # heuristic -> số instance vi phạm ràng buộc
    total = 0
    for path in sorted(glob.glob(os.path.join(data_dir, "*.json"))):
        name = os.path.basename(path)
        if name not in opt:
            continue
        with open(path, encoding="utf-8") as f:
            inst = json.load(f)
        total += 1
        for h, (e, feas) in heuristic_energies(inst).items():
            e_opt, cfg = opt[name]
            saved[h, inst["m"]].append((cfg, (e - e_opt) / e * 100))  # công thức của src/main_heuristics.cpp
            infeasible[h] += not feas

    fig, ax = plt.subplots(figsize=ps.COL)
    width = 0.2
    x = np.arange(len(MS))
    for k, h in enumerate(ps.HEURISTICS):
        mu = [mean_std(saved[h, m])[0] for m in MS]
        sd = [mean_std(saved[h, m])[1] for m in MS]
        st = ps.HEURISTICS[h]
        ax.bar(x + (k - 1.5) * width, mu, width, yerr=sd, capsize=1.5, color=st["color"], hatch=st["hatch"],
               edgecolor="black", linewidth=0.4, error_kw={"elinewidth": 0.6}, label=ps.HEURISTIC_LABELS[h])
    ax.set_xticks(x, [str(m) for m in MS])
    ax.set(xlabel="Số node tối đa $M$", ylabel="Energy Saved (%)", ylim=(0, 105))
    ax.grid(axis="x", visible=False)
    ax.legend(ncol=4, loc="lower center", bbox_to_anchor=(0.5, 1.0), fontsize=6, handlelength=1.4, columnspacing=0.8)
    ps.save(fig, "fig_energy_saved")

    print(f"\nEnergy Saved % (bậc ba, {total} instance, mean ± std theo M):")
    for h in ps.HEURISTICS:
        cells = "  ".join(f"M={m}: {mean_std(saved[h, m])[0]:6.2f} ± {mean_std(saved[h, m])[1]:5.2f}" for m in MS)
        print(f"  {h:<11} {cells}   vi phạm ràng buộc: {infeasible[h]}/{total}")


def print_tables(data):
    for model, _, solvers in MODELS:
        print(f"\n{MODEL_TITLE[model]}, M = {M_FIXED}: thời gian (ms) và peak RSS (MB), mean ± std seed theo N")
        for s in solvers:
            for n in NS:
                rows = [r for r in data[model] if r["solver"] == s and r["n"] == n]
                if not rows:
                    print(f"  {s:<9} N={n:<6} (không có lần chạy thành công)")
                    continue
                rows = [r for r in rows if r["m"] == M_FIXED]
                t, ts = mean_std([(r["cfg"], r["time_ms"]) for r in rows])
                mem, ms_ = mean_std([(r["cfg"], r["peak_rss_mb"]) for r in rows])
                print(f"  {s:<9} N={n:<6} {t:12.2f} ± {ts:10.2f}   {mem:7.2f} ± {ms_:5.2f}   ({len(rows)} lần)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--results", default="results")
    ap.add_argument("--data", default=os.path.join("data", "synthetic"))
    args = ap.parse_args()

    _selfcheck()
    ps.apply()
    data = {model: load(os.path.join(args.results, fname)) for model, fname, _ in MODELS}
    fig_time(data)
    fig_speedup(data)
    fig_memory(data)
    fig_energy_saved(data, args.data)
    print_tables(data)
    print("\nĐã lưu hình vào", os.path.normpath(ps.FIG_DIR))


if __name__ == "__main__":
    main()
