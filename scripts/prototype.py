#!/usr/bin/env python3
"""Prototype Python của baseline DP (mục 4 tài liệu mô hình 1.05).

Mục đích: kiểm tra nhanh công thức Cost(k+1, i) và công thức truy hồi
    DP(i, j) = min_{j-1 <= k < i} { DP(k, j-1) + Cost(k+1, i) }
bằng Python trước khi cài C++. Đây là việc 2.04 của TV4.

Không phụ thuộc thư viện ngoài; chỉ dùng math + json từ chuẩn.

Dùng:
    python3 scripts/prototype.py                          # chạy ví dụ mục 6
    python3 scripts/prototype.py path/to/instance.json    # chạy một input
    python3 scripts/prototype.py --stress 200             # sinh N=8 random và kiểm tra
Hàm `baseline_dp(n, m, w, d, params)` có thể import từ script khác:
    from prototype import baseline_dp, cost, build_prefix
"""
from __future__ import annotations

import argparse
import json
import math
import os
import random
import sys
from dataclasses import dataclass
from typing import Iterable, List, Optional, Tuple


INF = math.inf


@dataclass
class Params:
    kappa_over_T2: float = 1.0   # κ / T^2
    e_on: float = 0.0           # E_on: năng lượng bật một node trong một chu kỳ
    eps: float = 0.0            # ε = P_tx / B (J/bit)
    cap: float = 0.0            # f_max * T
    link_cap: float = 0.0       # B * T


@dataclass
class Instance:
    n: int
    m: int
    w: List[int]
    d: List[int]
    params: Params

    @classmethod
    def from_dict(cls, obj: dict) -> "Instance":
        p = obj.get("params", {})
        return cls(
            n=int(obj["n"]),
            m=int(obj["m"]),
            w=[int(x) for x in obj["w"]],
            d=[int(x) for x in obj["d"]],
            params=Params(
                kappa_over_T2=float(p["kappa_over_T2"]),
                e_on=float(p["e_on"]),
                eps=float(p["eps"]),
                cap=float(p["cap"]),
                link_cap=float(p["link_cap"]),
            ),
        )


def build_prefix(w: Iterable[int]) -> List[int]:
    """S_i = w_1 + ... + w_i, S_0 = 0. S là mảng size n+1."""
    s = [0]
    for x in w:
        s.append(s[-1] + int(x))
    return s


def cost(k: int, i: int, S: List[int], d: List[int], p: Params) -> float:
    """Cost(k+1, i) = E_on + (κ/T²) * (S_i - S_k)^3 + ε * d_k; +inf nếu vi phạm ràng buộc."""
    w = S[i] - S[k]
    if w > p.cap:
        return INF
    if k >= 1 and d[k] > p.link_cap:
        return INF
    return p.e_on + p.kappa_over_T2 * (float(w) ** 3) + p.eps * float(d[k])


def baseline_dp(
    in_: Instance,
    *,
    return_cuts: bool = True,
) -> Tuple[float, bool, List[int]]:
    """DP gốc: trả (energy, feasible, cuts_tracking).

    cuts là chỉ số 1-index của các điểm cắt (c_1..c_{j-1}), c_0=0, c_j=N ẩn.
    """
    n, m = in_.n, in_.m
    if n == 0:
        return 0.0, True, []
    if m == 0:
        return INF, False, []
    S = build_prefix(in_.w)
    p = in_.params

    # DP hai hàng + opt cho truy vết.
    prev = [INF] * (n + 1)
    prev[0] = 0.0
    opt = [[-1] * (n + 1) for _ in range(m + 1)]
    energy_per_j = [INF] * (m + 1)

    for j in range(1, m + 1):
        cur = [INF] * (n + 1)
        for i in range(j, n + 1):
            best = INF
            arg = -1
            for k in range(j - 1, i):
                if prev[k] == INF:
                    continue
                c = cost(k, i, S, in_.d, p)
                if c == INF:
                    continue
                v = prev[k] + c
                if v < best:
                    best = v
                    arg = k
            cur[i] = best
            opt[j][i] = arg
        energy_per_j[j] = cur[n]
        prev = cur

    best_e = INF
    jbest = -1
    for j in range(1, m + 1):
        if energy_per_j[j] < best_e:
            best_e = energy_per_j[j]
            jbest = j
    if jbest < 0:
        return INF, False, []

    if not return_cuts:
        return best_e, True, []

    # Truy vết cắt từ (N, jbest).
    cuts: List[int] = []
    i = n
    for j in range(jbest, 0, -1):
        k = opt[j][i]
        cuts.append(k)  # k là chỉ số tiền tố, cắt ngay sau τ_k
        i = k
    cuts.reverse()
    # Bỏ c_0 = 0 và c_j = N ẩn: chỉ giữ c_1..c_{j-1} (k > 0 và k < n).
    cuts = [c for c in cuts if 0 < c < n]
    return best_e, True, cuts


def pretty(in_: Instance, energy: float, feasible: bool, cuts: List[int]) -> str:
    segs: List[Tuple[int, int]] = []
    if feasible and in_.n > 0:
        prev = 0
        for c in cuts + [in_.n]:
            segs.append((prev + 1, c))
            prev = c
    return (
        f"feasible={feasible} energy={energy:g} nodes_used="
        f"{max(1 if feasible and in_.n else 0, len(cuts) + (1 if feasible and in_.n else 0))} cuts={cuts}\n"
        f"segments={segs}"
    )


def load_instance(path: str) -> Instance:
    with open(path, "r", encoding="utf-8") as f:
        return Instance.from_dict(json.load(f))


# ---------- Ví dụ mục 6 của tài liệu ----------
def example_n5() -> Instance:
    return Instance(
        n=5,
        m=3,
        w=[2, 3, 1, 4, 2],
        d=[3, 4, 2, 6, 1],
        params=Params(kappa_over_T2=1.0, e_on=250.0, eps=1.0, cap=8.0, link_cap=5.0),
    )


def _assert_close(a: float, b: float, tol: float = 1e-9) -> None:
    if not math.isfinite(a) and not math.isfinite(b):
        return
    if abs(a - b) > tol * max(1.0, abs(a)):
        raise AssertionError(f"khong khop: {a} vs {b} (tol={tol})")


def stress(num: int = 200, seed: int = 42) -> None:
    """Tự kiểm: brute force Python (O(2^(N-1)*N)) vs baseline, N <= 9, ngẫu nhiên."""
    rng = random.Random(seed)
    n_pass, n_wrong = 0, 0
    for t in range(num):
        n = rng.randint(1, 9)
        m = rng.randint(1, 6)
        w = [rng.randint(0, 8) for _ in range(n)]
        d = [rng.randint(0, 8) for _ in range(n)]
        params = Params(
            kappa_over_T2=rng.choice([0.5, 1.0, 2.0]),
            e_on=float(rng.randint(0, 300)),
            eps=rng.choice([0.5, 1.0, 2.0]),
            cap=float(rng.randint(8, 30)),
            link_cap=float(rng.randint(3, 10)),
        )
        in_ = Instance(n=n, m=m, w=w, d=d, params=params)
        energy_b, feas_b, _ = baseline_dp(in_)

        # Brute force: mọi mask cắt, tối đa m-1 điểm.
        if n == 0:
            energy_a, feas_a = 0.0, True
        elif m == 0:
            energy_a, feas_a = INF, False
        else:
            S = build_prefix(w)
            best, ok = INF, False
            for mask in range(1 << (n - 1)):
                if bin(mask).count("1") >= m:
                    continue
                cur, prev, fea = 0.0, 0, True
                for i in range(1, n + 1):
                    c = cost(prev, i, S, d, params)
                    if c == INF:
                        fea = False
                        break
                    cur += c
                    # chi cap nhat prev khi cat tai i hoac ket thuc chuoi
                    if i == n or (mask >> (i - 1)) & 1:
                        prev = i
            energy_a, feas_a = (best if ok else INF), ok

        if feas_a != feas_b or (feas_a and not _within(energy_a, energy_b)):
            n_wrong += 1
            print(f"[STRESS FAIL t={t}] n={n} m={m} brute=({energy_a},{feas_a}) baseline=({energy_b},{feas_b})")
        else:
            n_pass += 1
    print(f"[STRESS] {n_pass}/{num} pass, {n_wrong} fail")


def _within(a: float, b: float, tol: float = 1e-9) -> bool:
    if math.isinf(a) and math.isinf(b):
        return True
    return abs(a - b) <= tol * max(1.0, abs(a))


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(description="Prototype Python của baseline DP.")
    ap.add_argument("path", nargs="?", help="file JSON instance (tùy chọn)")
    ap.add_argument("--stress", type=int, default=0, help="số vòng test ngẫu nhiên so với brute force")
    ap.add_argument("--seed", type=int, default=42)
    args = ap.parse_args(argv)

    if args.stress > 0:
        stress(args.stress, args.seed)
        return 0

    if args.path:
        in_ = load_instance(args.path)
    else:
        in_ = example_n5()
        print("Chạy ví dụ mục 6 tài liệu mô hình 1.05 (đáp án kỳ vọng: 973, 2 node, cắt [2]).")

    energy, feas, cuts = baseline_dp(in_)
    print(pretty(in_, energy, feas, cuts))
    return 0


if __name__ == "__main__":
    sys.exit(main())
