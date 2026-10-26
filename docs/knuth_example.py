"""Knuth Optimization minh hoạ: bài toán nối dây cổ điển.

DP[i][j] = min_{k in [opt[i][j-1], opt[i+1][j]]} { DP[i][k] + DP[k][j] + C[i][j] }
Với C[i][j] = a[i] + ... + a[j] (đơn điản để C thoả điều kiện Knuth).

Chạy: python3 docs/knuth_example.py [n]
"""
import sys


def solve(prefix):
    n = len(prefix) - 1
    INF = float("inf")
    dp = [[0] * (n + 1) for _ in range(n + 1)]
    opt = [[0] * (n + 1) for _ in range(n + 1)]
    for i in range(n + 1):
        opt[i][i] = i
        dp[i][i] = 0
    for length in range(2, n + 1):
        for i in range(n - length + 1):
            j = i + length
            best = INF
            best_k = -1
            klo = opt[i][j - 1]
            khi = opt[i + 1][j]
            for k in range(klo, khi + 1):
                val = dp[i][k] + dp[k][j] + (prefix[j] - prefix[i])
                if val < best:
                    best = val
                    best_k = k
            dp[i][j] = best
            opt[i][j] = best_k
    return dp[0][n]


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 20
    a = list(range(1, n * 2 + 1, 2))
    prefix = [0]
    for x in a:
        prefix.append(prefix[-1] + x)
    print(f"Knuth DP trên N = {n}: cost = {solve(prefix)}")


if __name__ == "__main__":
    main()
