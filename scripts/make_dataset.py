#!/usr/bin/env python3
"""Task 5.01 - full experiment grid: N in {1e3,1e4,1e5} x M in {4,8,16,32} x 5 seeds (CSV)."""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from gen_data import gen, write
out = sys.argv[1] if len(sys.argv) > 1 else "data"
os.makedirs(out, exist_ok=True)
for n in (10**3, 10**4, 10**5):
    for m in (4, 8, 16, 32):
        for s in range(1, 6):
            write(gen(n, m, s), f"{out}/syn_n{n}_m{m}_s{s}.csv")
print("done")
