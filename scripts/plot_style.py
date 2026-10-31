#!/usr/bin/env python3
"""Style matplotlib chung cho mọi đồ thị của báo cáo (IEEE conference, tiếng Việt).

Dùng:
    import plot_style
    plot_style.apply()
    fig, ax = plt.subplots(figsize=plot_style.COL)
    ax.loglog(N, t, **plot_style.solver("cht"), label=plot_style.LABELS["cht"])
    plot_style.save(fig, "runtime_vs_n")      # -> figures/runtime_vs_n.pdf + .png

Chạy trực tiếp để sinh đồ thị mẫu kiểm tra:
    python3 scripts/plot_style.py
"""
from __future__ import annotations

import os

import matplotlib as mpl
import matplotlib.pyplot as plt

# Kích thước theo khổ IEEE: 1 cột rộng 3.5 in, 2 cột 7.16 in.
COL = (3.5, 2.4)
WIDE = (7.16, 2.6)

# Màu Okabe-Ito (an toàn cho người mù màu, in đen trắng vẫn phân biệt nhờ marker).
# Mỗi solver cố định một màu/marker để mọi đồ thị nhất quán.
SOLVERS = {
    "brute":     {"color": "#000000", "marker": "x", "linestyle": ":"},
    "baseline":  {"color": "#D55E00", "marker": "o", "linestyle": "-"},
    "dc":        {"color": "#0072B2", "marker": "s", "linestyle": "-"},
    "cht":       {"color": "#009E73", "marker": "^", "linestyle": "-"},
    "wqs":       {"color": "#CC79A7", "marker": "D", "linestyle": "-"},
    "heuristic": {"color": "#E69F00", "marker": "v", "linestyle": "--"},
}
LABELS = {
    "brute": "Vét cạn",
    "baseline": r"DP gốc $O(MN^2)$",
    "dc": r"D&C $O(MN\log N)$",
    "cht": r"CHT $O(MN)$",
    "wqs": r"WQS $O(N\log N\log C)$",
    "heuristic": "Heuristic",
}

# 4 heuristic so sánh Energy Saved % (src/heuristics.cpp); màu khác các solver DP ở trên.
HEURISTICS = {
    "greedy":     {"color": "#E69F00", "hatch": ""},
    "equal":      {"color": "#56B4E9", "hatch": "//"},
    "single":     {"color": "#999999", "hatch": "xx"},
    "roundrobin": {"color": "#CC79A7", "hatch": ".."},
}
HEURISTIC_LABELS = {"greedy": "Greedy", "equal": "Chia đều", "single": "Một node", "roundrobin": "Round-robin"}

FIG_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "figures")

RC = {
    # Font serif có đủ dấu tiếng Việt; Times cho giống thân bài IEEE, DejaVu làm dự phòng.
    "font.family": "serif",
    "font.serif": ["Times New Roman", "Times", "DejaVu Serif"],
    "mathtext.fontset": "stix",
    "font.size": 8,
    "axes.titlesize": 8,
    "axes.labelsize": 8,
    "legend.fontsize": 7,
    "xtick.labelsize": 7,
    "ytick.labelsize": 7,
    "axes.prop_cycle": mpl.cycler(color=[s["color"] for s in SOLVERS.values()]),
    "axes.grid": True,
    "axes.grid.which": "both",
    "grid.alpha": 0.3,
    "grid.linewidth": 0.4,
    "axes.linewidth": 0.6,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "lines.linewidth": 1.2,
    "lines.markersize": 4,
    "legend.frameon": False,
    "figure.dpi": 150,
    "savefig.dpi": 300,
    "savefig.bbox": "tight",
    "savefig.pad_inches": 0.02,
    # TrueType trong PDF để Overleaf/IEEE PDF eXpress không báo lỗi font Type 3.
    "pdf.fonttype": 42,
    "ps.fonttype": 42,
}


def apply() -> None:
    """Áp style cho toàn bộ phiên matplotlib hiện tại."""
    mpl.rcParams.update(RC)


def solver(name: str) -> dict:
    """Kwargs màu/marker/linestyle cố định cho một solver, truyền thẳng vào ax.plot."""
    return dict(SOLVERS[name])


def save(fig, name: str, out_dir: str = FIG_DIR) -> None:
    """Lưu cả PDF (chèn báo cáo) và PNG (slide) vào figures/."""
    os.makedirs(out_dir, exist_ok=True)
    for ext in ("pdf", "png"):
        fig.savefig(os.path.join(out_dir, f"{name}.{ext}"))


if __name__ == "__main__":
    apply()
    n = [10**k for k in range(1, 6)]
    fig, ax = plt.subplots(figsize=COL)
    for key, f in [("baseline", lambda x: x * x), ("dc", lambda x: x * 17),
                   ("cht", lambda x: x * 2), ("heuristic", lambda x: x)]:
        ax.loglog(n, [f(x) * 1e-6 for x in n], **solver(key), label=LABELS[key])
    ax.set_xlabel("Số tác vụ $N$")
    ax.set_ylabel("Thời gian chạy (s)")
    ax.set_title("Đồ thị mẫu — kiểm tra dấu tiếng Việt")
    ax.legend()
    save(fig, "_style_demo")
    # ASCII để không lỗi trên console Windows cp1252.
    print("Saved",os.path.normpath(os.path.join(FIG_DIR, "_style_demo.{pdf,png}")))
