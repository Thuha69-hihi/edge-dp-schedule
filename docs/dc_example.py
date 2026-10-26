"""Minh hoạ Divide & Conquer DP cho bài toán DP[i][j] = min_{k<i} { DP[k][j-1] + C(k,i) }
với C(k, i) = (i - k)^2 (Cost đơn điản). Đây là bài toán cổ điển áp dụng D&C.

Chạy: python3 docs/dc_example.py [n]
"""
import sys


def cost(k, i):
    return (i - k) ** 2


def compute(j, lo, hi, optL, optR, prev, cur, opt):
    if lo > hi:
        return
    mid = (lo + hi) // 2
    best = None
    arg = -1
    klo = max(j - 1, optL)
    khi = min(mid - 1, optR)
    for k in range(klo, khi + 1):
        val = prev[k] + cost(k, mid)
        if best is None or val < best:
            best = val
            arg = k
    cur[mid] = best
    opt[mid] = arg
    compute(j, lo, mid - 1, optL, arg, prev, cur, opt)
    compute(j, mid + 1, hi, arg, optR, prev, cur, opt)


def solve(n, m):
    prev = [float("inf")] * (n + 1)
    prev[0] = 0
    opt_table = [[-1] * (n + 1) for _ in range(m + 1)]
    for j in range(1, m + 1):
        cur = [float("inf")] * (n + 1)
        compute(j, j, n, j - 1, n - 1, prev, cur, opt_table[j])
        prev = cur

    # E* = min_j DP[N][j]
    best = min(prev[n], prev[n])  # prev sau vòng cuối là hàng m
    return best, opt_table


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 20
    m = 3
    energy, opt = solve(n, m)
    print(f"D&C DP trên N = {n}, M = {m}: energy = {energy}")
    print("opt_table hàng cuối (j = m):", opt[m][:])


if __name__ == "__main__":
    main()
