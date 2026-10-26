"""Minh hoạ Convex Hull Trick (deque) cho tác vụ nhỏ, độc lập với repo.

Bài toán: cho dãy a[1..N] xét DP[i] = min_{k < i} { DP[k] + m_k * x_i + b_k } với
m_k = -2 k, x_i = i, b_k = k^2. (Đây là một biến thể đơn giản hoá.

Hệ số góc m_k giảm dần theo k, điểm truy vấn x_i tăng dần theo i → deque, O(1) amortized.

Chạy: python3 docs/cht_example.py
"""
from collections import deque
from fractions import Fraction


class Line:
    __slots__ = ("m", "b")

    def __init__(self, m, b):
        self.m = Fraction(m)
        self.b = Fraction(b)

    def value(self, x):
        return self.m * x + self.b


def is_bad(l1, l2, l3):
    # (l3.b - l1.b) / (l1.m - l3.m) <= (l2.b - l1.b) / (l1.m - l2.m)
    # Tránh tràn số bằng phép nhân chéo:
    return (l3.b - l1.b) * (l1.m - l2.m) >= (l2.b - l1.b) * (l1.m - l3.m)


def cht_query(deq, x):
    while len(deq) >= 2 and deq[0].value(x) >= deq[1].value(x):
        deq.popleft()
    return deq[0].value(x)


def cht_add(deq, m, b):
    new = Line(m, b)
    while len(deq) >= 2 and is_bad(deq[-2], deq[-1], new):
        deq.pop()
    deq.append(new)


def solve(n):
    dp = [0] * (n + 1)
    deq = deque()
    # init với dòng tại k=0
    cht_add(deq, 0, 0)
    for i in range(1, n + 1):
        x = Fraction(i)
        dp[i] = int(cht_query(deq, x))
        # Thêm đường thẳng cho k = i vào deque cho các i sau dùng
        cht_add(deq, -2 * i, i * i)
    return dp


def main():
    n = 10
    dp = solve(n)
    print("CHT (deque) O(N) trên N =", n)
    for i, v in enumerate(dp):
        print(f"  DP[{i}] = {v}")


if __name__ == "__main__":
    main()
