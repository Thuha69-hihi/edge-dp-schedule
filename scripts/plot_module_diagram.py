#!/usr/bin/env python3
"""Vẽ Hình 5.1: sơ đồ module của chương trình (theo docs/design.md, mục 1).

Dùng:
    python3 scripts/plot_module_diagram.py      # -> figures/fig_module_diagram.{pdf,png}
"""
from __future__ import annotations

import os
import sys

import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import plot_style as ps  # noqa: E402

W, H = 1.45, 0.62  # kích thước một khối (đơn vị trục)
FILL = {"data": "#FFFFFF", "core": "#E8F1FA", "solver": "#DCEFE8", "tool": "#F5F5F5"}

# tên: (x tâm, y tâm, dòng 1, dòng 2, loại)
NODES = {
    "json":  (0.85, 2.0, "File JSON", "(Instance)", "data"),
    "io":    (2.6, 2.0, "io + json", "đọc / ghi JSON", "core"),
    "inst":  (4.35, 2.0, "Instance", "", "data"),
    "reg":   (6.1, 2.0, "registry", "all_solvers()", "core"),
    "cost":  (8.0, 4.15, "cost.hpp", "CostModel", "core"),
    "base":  (8.0, 3.0, "baseline_dp", "DP gốc", "solver"),
    "dc":    (8.0, 2.2, "dc_opt", "D&C", "solver"),
    "cht":   (8.0, 1.4, "cht_dp", "CHT", "solver"),
    "bf":    (8.0, 0.6, "brute_force", "vét cạn", "solver"),
    "sol":   (9.9, 1.8, "Solution", "", "data"),
    "cli":   (11.65, 1.8, "edgedp (CLI)", "đo giờ, in JSON", "core"),
    "bench": (13.4, 1.8, "run_bench", "thời gian, RSS", "tool"),
    "heur":  (6.1, -0.45, "heuristics", "4 heuristic", "tool"),
    "hcli":  (9.9, -0.45, "edgedp_heur", "Energy Saved %", "tool"),
}
SOLVERS = ["base", "dc", "cht", "bf"]
EDGES = ([("json", "io"), ("io", "inst"), ("inst", "reg")]
         + [("reg", s) for s in SOLVERS] + [(s, "sol") for s in SOLVERS]
         + [("sol", "cli"), ("cli", "bench"), ("inst", "heur"), ("heur", "hcli")])


def box(ax, key):
    x, y, l1, l2, kind = NODES[key]
    ax.add_patch(FancyBboxPatch((x - W / 2, y - H / 2), W, H, boxstyle="round,pad=0.02,rounding_size=0.08",
                                facecolor=FILL[kind], edgecolor="black", linewidth=0.7))
    if l2:
        ax.text(x, y + 0.11, l1, ha="center", va="center", fontsize=7, fontweight="bold")
        ax.text(x, y - 0.13, l2, ha="center", va="center", fontsize=6)
    else:
        ax.text(x, y, l1, ha="center", va="center", fontsize=7, fontweight="bold")


def edge(ax, a, b, style="-"):
    (xa, ya), (xb, yb) = NODES[a][:2], NODES[b][:2]
    # đi ra từ cạnh phải của a, vào cạnh trái của b (hoặc cạnh trên/dưới khi cùng cột)
    if abs(xa - xb) < 1e-6:
        start, end = (xa, ya + (H / 2 if yb > ya else -H / 2)), (xb, yb + (-H / 2 if yb > ya else H / 2))
    elif abs(ya - yb) > 1.5 and a in ("inst",):
        start, end = (xa, ya - H / 2), (xb - W / 2, yb)
    else:
        start, end = (xa + W / 2, ya), (xb - W / 2, yb)
    conn = "angle,angleA=-90,angleB=180,rad=0" if start[0] == xa and start[0] != end[0] else "arc3,rad=0"
    ax.annotate("", xy=end, xytext=start,
                arrowprops=dict(arrowstyle="-|>", lw=0.7, color="black", linestyle=style,
                                shrinkA=0, shrinkB=0, mutation_scale=7, connectionstyle=conn))


def main():
    ps.apply()
    fig, ax = plt.subplots(figsize=(ps.WIDE[0], 3.0))
    for a, b in EDGES:
        edge(ax, a, b)
    # 4 solver dùng chung CostModel: khung nét đứt quanh cột solver, cost.hpp trỏ vào khung
    x0, y_top, y_bot = NODES["base"][0], NODES["base"][1] + H / 2 + 0.12, NODES["bf"][1] - H / 2 - 0.12
    ax.add_patch(FancyBboxPatch((x0 - W / 2 - 0.12, y_bot), W + 0.24, y_top - y_bot,
                                boxstyle="round,pad=0,rounding_size=0.1", fill=False, lw=0.6, ls="--"))
    ax.annotate("", xy=(x0, y_top), xytext=(x0, NODES["cost"][1] - H / 2),
                arrowprops=dict(arrowstyle="-|>", lw=0.7, ls="--", shrinkA=0, shrinkB=0, mutation_scale=7))
    for k in NODES:
        box(ax, k)
    ax.set_xlim(0, 14.2)
    ax.set_ylim(-0.95, 4.6)
    ax.set_aspect("equal")
    ax.axis("off")
    ps.save(fig, "fig_module_diagram")


if __name__ == "__main__":
    main()
