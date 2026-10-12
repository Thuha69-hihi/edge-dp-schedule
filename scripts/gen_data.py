#!/usr/bin/env python3
"""Task 2.06 - IoT task-chain data generator (TV5).

Model (see 'Ghi chu ky thuat' #1,#2): N tasks in a sequential chain, split into
<= M contiguous segments, one per Edge node.
  w_i : CPU cycles of task i (workload)
  d_i : bytes sent from task i to task i+1 (transfer at segment boundary)
  dl_i: per-task deadline offset; segment (k,i] must finish by deadline
Outputs JSON or CSV. Fixed seed -> reproducible.

Usage:
  python3 gen_data.py --n 1000 --m 8 --seed 1 --dist lognormal --out data/n1000_m8_s1.json
  python3 gen_data.py --trace cluster.csv --n 1000 --out data/trace.json   # Google trace mapping (5.01)
"""
import argparse, csv, json, random, math, sys

DISTS = ("uniform", "lognormal", "pareto", "bimodal")

def draw(rng, dist, mean):
    if dist == "uniform":   return rng.uniform(0.2 * mean, 1.8 * mean)
    if dist == "lognormal":
        s = 0.8; return rng.lognormvariate(math.log(mean) - s * s / 2, s)
    if dist == "pareto":
        a = 2.5; return mean * (a - 1) / a * rng.paretovariate(a)
    if dist == "bimodal":   return mean * (0.3 if rng.random() < 0.7 else 3.4) * rng.uniform(.8, 1.2)
    raise ValueError(dist)

def gen(n, m, seed, dist="lognormal", w_mean=5e7, d_mean=2e4, slack=1.5,
        f_max=1e9, trace_w=None):
    rng = random.Random(seed)
    if trace_w:
        w = [trace_w[i % len(trace_w)] for i in range(n)]
    else:
        w = [max(1.0, round(draw(rng, dist, w_mean))) for _ in range(n)]
    d = [max(1.0, round(draw(rng, "lognormal", d_mean))) for _ in range(n)]
    d[-1] = 0                       # last task sends nothing
    # deadline: with m nodes at max freq an even split needs ~sum(w)/(m*f_max); give `slack` margin
    total = sum(w)
    horizon = slack * total / (m * f_max)
    # cumulative per-task deadline (seconds) grows linearly with prefix workload -> feasible chain
    S = 0.0; dl = []
    for wi in w:
        S += wi
        dl.append(round(slack * (S / f_max) + horizon / m, 9))
    return {"n": n, "m": m, "seed": seed, "dist": dist, "f_max": f_max,
            "slack": slack, "w": w, "d": d, "deadline": dl}

def load_trace(path, col="cpu_cycles"):
    out = []
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            try: out.append(float(row[col]))
            except (KeyError, ValueError): pass
    if not out: sys.exit("trace has no usable rows")
    return out

def write(inst, path):
    if path.endswith(".csv"):
        with open(path, "w", newline="") as f:
            wr = csv.writer(f); wr.writerow(["i", "w", "d", "deadline"])
            for i in range(inst["n"]):
                wr.writerow([i + 1, inst["w"][i], inst["d"][i], inst["deadline"][i]])
    else:
        with open(path, "w") as f: json.dump(inst, f)

if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--n", type=int, required=True); p.add_argument("--m", type=int, default=8)
    p.add_argument("--seed", type=int, default=1); p.add_argument("--dist", choices=DISTS, default="lognormal")
    p.add_argument("--slack", type=float, default=1.5); p.add_argument("--trace")
    p.add_argument("--trace-col", default="cpu_cycles"); p.add_argument("--out", required=True)
    a = p.parse_args()
    tw = load_trace(a.trace, a.trace_col) if a.trace else None
    write(gen(a.n, a.m, a.seed, a.dist, slack=a.slack, trace_w=tw), a.out)
    print(f"wrote {a.out}")
