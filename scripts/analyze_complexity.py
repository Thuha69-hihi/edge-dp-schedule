#!/usr/bin/env python3
"""Việc 5.05: đối chiếu thời gian chạy thực nghiệm (5.02) với độ phức tạp lý thuyết.

Đọc results/time_mem.csv (mô hình bậc hai) và results/time_mem_cubic.csv (bậc ba), rồi:
  1. Bỏ các dòng không phải đo thật: status != ok (timeout, not_impl). Riêng baseline N = 10^5
     chỉ là ngoại suy 100 x thời gian N = 10^4 nên KHÔNG được dùng để ước lượng độ dốc
     (nếu dùng, độ dốc 2 đúng bằng cách dựng chứ không phải đo).
  2. Gộp các lần lặp (rep) của cùng cấu hình và N bằng trung vị, rồi với mỗi cấu hình
     (mô hình, solver, M, alpha, beta, seed) hồi quy
        log10(time_ms) = b * log10(N) + c
     trên các N đo được; b là độ dốc log-log.
  3. Tổng hợp b theo solver: trung bình, độ lệch chuẩn, khoảng tin cậy 95% (Student t) của trung bình;
     tách CHT theo nhánh cài đặt (deque khi cap >= S_N, cây phân đoạn khi có cửa sổ).
  4. Tính hằng số ẩn: ns trên mỗi "phép toán". D&C, CHT: M*N*log2 N. DP gốc (đã cắt tỉa cửa sổ cap,
     PR #14): M*N*min(N/2, L) với L = alpha*N/M là số tác vụ trung bình vừa một cửa sổ cap = alpha*S_N/M.
  5. Vẽ figures/loglog_time_vs_n.{pdf,png} theo scripts/plot_style.py.

Chạy:  python3 scripts/analyze_complexity.py
Ra:    results/complexity_slopes.csv, results/complexity_summary.csv, figures/loglog_time_vs_n.{pdf,png}
"""
from __future__ import annotations

import math
import os
import sys

import numpy as np
import pandas as pd

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
sys.path.insert(0, os.path.join(ROOT, "scripts"))

INPUTS = [("quadratic", "results/time_mem.csv"), ("cubic", "results/time_mem_cubic.csv")]
KEY = ["model", "solver", "m", "alpha", "beta", "seed"]


def t_crit(dof: int) -> float:
    """Phân vị 0,975 của phân phối Student t (dùng scipy nếu có, không thì xấp xỉ chuẩn 1,96)."""
    try:
        from scipy.stats import t
        return float(t.ppf(0.975, dof))
    except ImportError:
        return 1.96


def nlogn_ref_slope(n1: float, n2: float) -> float:
    """Độ dốc log-log 'cát tuyến' của N log N giữa n1 và n2 (đường N log N không thẳng trên log-log)."""
    return math.log10((n2 * math.log(n2)) / (n1 * math.log(n1))) / math.log10(n2 / n1)


def load() -> pd.DataFrame:
    frames = []
    for model, rel in INPUTS:
        df = pd.read_csv(os.path.join(ROOT, rel))
        df["model"] = model
        frames.append(df)
    df = pd.concat(frames, ignore_index=True)
    # Cấu hình chạy nhánh deque của CHT: cửa sổ không bao giờ cắt khi cap = alpha*S_N/M >= S_N, tức alpha >= M.
    df["cht_branch"] = np.where(df["alpha"] >= df["m"], "deque", "tree")
    return df


def per_config_slopes(ok: pd.DataFrame) -> pd.DataFrame:
    rows = []
    for k, g in ok.groupby(KEY):
        x, y = np.log10(g["n"].to_numpy(float)), np.log10(g["time_ms"].to_numpy(float))
        b = np.polyfit(x, y, 1)[0]
        rows.append(dict(zip(KEY, k), slope=b, points=len(g), ns=",".join(str(int(v)) for v in sorted(g["n"]))))
    s = pd.DataFrame(rows)
    s["cht_branch"] = np.where(s["alpha"] >= s["m"], "deque", "tree")
    return s


def summarize(s: pd.DataFrame) -> pd.DataFrame:
    def agg(g: pd.DataFrame) -> pd.Series:
        n = len(g)
        mean, sd = g["slope"].mean(), g["slope"].std(ddof=1)
        half = t_crit(n - 1) * sd / math.sqrt(n)
        return pd.Series({"configs": n, "slope_mean": mean, "slope_std": sd,
                          "ci95_lo": mean - half, "ci95_hi": mean + half,
                          "slope_min": g["slope"].min(), "slope_max": g["slope"].max()})

    parts = [s.groupby(["model", "solver"]).apply(agg, include_groups=False).reset_index().assign(subset="all")]
    cht = s[s["solver"] == "cht"]
    if len(cht):
        parts.append(cht.groupby(["model", "solver", "cht_branch"]).apply(agg, include_groups=False)
                     .reset_index().rename(columns={"cht_branch": "subset"}))
    out = pd.concat(parts, ignore_index=True)
    theory = {"baseline": 2.0, "dc": nlogn_ref_slope(1e3, 1e5), "cht": nlogn_ref_slope(1e3, 1e5)}
    out["theory"] = out["solver"].map(theory)
    out.loc[(out["solver"] == "cht") & (out["subset"] == "deque"), "theory"] = 1.0
    return out[["model", "solver", "subset", "configs", "theory", "slope_mean", "slope_std",
                "ci95_lo", "ci95_hi", "slope_min", "slope_max"]]


def collapse_reps(ok: pd.DataFrame) -> pd.DataFrame:
    """Một dòng cho mỗi (cấu hình, N): trung vị time_ms qua các lần lặp."""
    return ok.groupby(KEY + ["n", "cht_branch"], as_index=False)["time_ms"].median()


def hidden_constants(ok: pd.DataFrame) -> pd.DataFrame:
    window = np.minimum(ok["n"] / 2, ok["alpha"] * ok["n"] / ok["m"])
    ops = np.where(ok["solver"] == "baseline", ok["m"] * ok["n"] * window, ok["m"] * ok["n"] * np.log2(ok["n"]))
    t = ok.assign(ns_per_op=ok["time_ms"] * 1e6 / ops)
    return t.groupby(["model", "solver", "n"])["ns_per_op"].median().unstack()


def plot(df: pd.DataFrame, summary: pd.DataFrame) -> None:
    import matplotlib.pyplot as plt
    import plot_style as ps

    ps.apply()
    fig, axes = plt.subplots(1, 2, figsize=ps.WIDE, sharey=True)
    labels = {"baseline": r"DP gốc $O(MN^2)$", "dc": r"D&C $O(MN\log N)$", "cht": r"CHT $O(MN\log N)$ / $O(MN)$"}
    for ax, (model, title) in zip(axes, [("quadratic", "(a) Năng lượng bậc hai"), ("cubic", "(b) Năng lượng bậc ba")]):
        sub = df[df["model"] == model]
        for solver in ["baseline", "dc", "cht"]:
            g = sub[(sub["solver"] == solver)]
            if g.empty or (g["status"] == "ok").sum() == 0:
                continue
            st = ps.solver(solver)
            meas = collapse_reps(g[g["status"] == "ok"]).groupby("n")["time_ms"]
            q = meas.quantile([0.25, 0.5, 0.75]).unstack()
            n = q.index.to_numpy(float)
            ax.errorbar(n, q[0.5], yerr=[q[0.5] - q[0.25], q[0.75] - q[0.5]], color=st["color"],
                        marker=st["marker"], linestyle=st["linestyle"], capsize=2, elinewidth=0.6,
                        label=labels[solver])
            if solver == "baseline":
                ext = g[(g["status"] == "timeout") & g["time_ms"].notna()].groupby("n")["time_ms"].median()
                if len(ext):
                    ax.plot([n[-1], ext.index[0]], [q[0.5].iloc[-1], ext.iloc[0]], color=st["color"],
                            linestyle="--", linewidth=1.0)
                    ax.plot(ext.index, ext.values, marker=st["marker"], markerfacecolor="white",
                            color=st["color"], linestyle="none", label="DP gốc, ngoại suy")
            b = summary[(summary["model"] == model) & (summary["solver"] == solver) & (summary["subset"] == "all")]
            if len(b):
                txt = f"{b['slope_mean'].iloc[0]:.2f}".replace(".", ",")
                ax.annotate(f"độ dốc {txt}", (n[-1], q[0.5].iloc[-1]), xytext=(4, -9 if solver == "dc" else 2),
                            textcoords="offset points", fontsize=6.5, color="#333333")
        ax.set_xscale("log")
        ax.set_yscale("log")
        ax.set_xlim(6e2, 4e5)
        ax.set_title(title)
        ax.set_xlabel("Số tác vụ $N$")
    axes[0].set_ylabel("Thời gian giải (ms)")
    handles, lab = axes[0].get_legend_handles_labels()
    order = sorted(range(len(lab)), key=lambda i: ["DP gốc $", "DP gốc, ", "D&C", "CHT"].index(
        next(p for p in ["DP gốc $", "DP gốc, ", "D&C", "CHT"] if lab[i].startswith(p))))
    handles, lab = [handles[i] for i in order], [lab[i] for i in order]
    fig.legend(handles, lab, loc="lower center", ncol=4, bbox_to_anchor=(0.5, -0.08))
    fig.tight_layout()
    ps.save(fig, "loglog_time_vs_n", out_dir=os.path.join(ROOT, "figures"))


def main() -> None:
    df = load()
    ok_all = df[df["status"] == "ok"].copy()
    ok = collapse_reps(ok_all)
    print("So lan lap moi cau hinh:", sorted(df["rep"].dropna().unique().astype(int).tolist()))
    s = per_config_slopes(ok)
    summary = summarize(s)
    os.makedirs(os.path.join(ROOT, "results"), exist_ok=True)
    s.to_csv(os.path.join(ROOT, "results/complexity_slopes.csv"), index=False)
    summary.to_csv(os.path.join(ROOT, "results/complexity_summary.csv"), index=False, float_format="%.4f")

    pd.set_option("display.width", 160)
    print("Loai khoi hoi quy:", (df["status"] != "ok").sum(), "dong (timeout/ngoai suy, not_impl)")
    print("\nDo doc log-log theo cau hinh:")
    print(summary.round(3).to_string(index=False))
    print("\nTham chieu N log N: 1e3->1e4 %.3f, 1e4->1e5 %.3f, 1e3->1e5 %.3f"
          % (nlogn_ref_slope(1e3, 1e4), nlogn_ref_slope(1e4, 1e5), nlogn_ref_slope(1e3, 1e5)))
    med = ok.groupby(["model", "solver", "n"])["time_ms"].median().unstack()
    print("\nTrung vi time_ms:\n", med.round(3).to_string())
    print("\nDo doc cuc bo (trung vi):")
    for a, b in [(1000, 10000), (10000, 100000)]:
        if a in med and b in med:
            print(f"  {a}->{b}:", np.log10(med[b] / med[a]).round(3).to_dict())
    print("\nHang so an (ns / phep toan, trung vi):\n", hidden_constants(ok).round(2).to_string())
    print("\nTheo M:\n", s.groupby(["model", "solver", "m"])["slope"].mean().unstack().round(3).to_string())
    for (mo, so), g in ok.groupby(["model", "solver"]):
        nn = 10000 if so == "baseline" else 100000
        h = g[g["n"] == nn].groupby("m")["time_ms"].median()
        if len(h) > 1:
            print(f"Do doc theo M ({mo}, {so}, N={nn}): %.2f" % np.polyfit(np.log10(h.index), np.log10(h.values), 1)[0],
                  h.round(1).to_dict())
    print("\nTrung vi N=1e4 theo (solver, alpha):\n",
          ok[ok["n"] == 10000].groupby(["model", "solver", "alpha"])["time_ms"].median().unstack().round(1).to_string())
    # Kiểm tra: N = 10^5 của baseline có đúng là ngoại suy x100 không.
    b = df[df["solver"] == "baseline"].pivot_table(index=["model", "m", "alpha", "beta", "seed"],
                                                    columns="n", values="time_ms")
    if 100000 in b and 10000 in b:
        r = (b[100000] / b[10000]).round(6)
        print("\nBaseline 1e5/1e4 (phai = 100 neu la ngoai suy):", r.min(), "-", r.max())
    mism = df["match_ref"].dropna()
    print("Dong ok:", len(ok_all), "| sau khi gop lap:", len(ok))
    print("match_ref:", mism.value_counts().to_dict())
    plot(df, summary)
    print("\nDa ghi results/complexity_slopes.csv, results/complexity_summary.csv, figures/loglog_time_vs_n.{pdf,png}")


if __name__ == "__main__":
    main()
