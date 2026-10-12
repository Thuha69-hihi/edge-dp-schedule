#!/usr/bin/env python3
"""Task 3.06 - benchmark harness (TV5): time + peak memory, multi-seed, CSV output.

Solver contract: `<cmd> <input.csv> <M>` prints a line `RESULT energy=<float> [violations=<int>]`.
Time = wall-clock of the process (ms). Peak memory = ru_maxrss of that child (KB, via os.wait4).
Timeout -> row with status=timeout (use for baseline at N=1e5; extrapolate later with fit_loglog.py).

Examples:
  python3 bench/run_bench.py --solver dp=./build/baseline --solver dnc=./build/dnc --solver cht=./build/cht \
      --n 1000 --m 8 --seeds 1 2 3 4 5 --timeout 60 --out results/time_mem.csv
  python3 bench/run_bench.py --solver dp="./heuristics {in} {m} --only dp" ...   # {in},{m} placeholders optional
"""
import argparse, csv, os, re, shlex, signal, subprocess, sys, time, tempfile
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "scripts"))
from gen_data import gen, write

def run_one(cmd, timeout):
    t0 = time.perf_counter()
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, preexec_fn=os.setsid)
    deadline = t0 + timeout
    while True:
        pid, status, ru = os.wait4(p.pid, os.WNOHANG)
        if pid: break
        if time.perf_counter() > deadline:
            os.killpg(p.pid, signal.SIGKILL); os.wait4(p.pid, 0)
            return dict(status="timeout", time_ms=timeout * 1000, mem_kb="", energy="", violations="")
        time.sleep(0.002)
    ms = (time.perf_counter() - t0) * 1000
    out = p.stdout.read(); p.stdout.close(); p.stderr.close()
    m = re.search(r"RESULT\s+energy=(\S+)(?:\s+violations=(\S+))?", out)
    ok = os.WIFEXITED(status) and os.WEXITSTATUS(status) == 0 and m
    return dict(status="ok" if ok else "error", time_ms=round(ms, 3), mem_kb=ru.ru_maxrss,
                energy=m.group(1) if m else "", violations=(m.group(2) or "") if m else "")

def build_cmd(spec, inp, m):
    if "{in}" in spec or "{m}" in spec:
        return shlex.split(spec.replace("{in}", inp).replace("{m}", str(m)))
    return shlex.split(spec) + [inp, str(m)]

def main():
    a = argparse.ArgumentParser()
    a.add_argument("--solver", action="append", required=True, help="name=command")
    a.add_argument("--n", type=int, nargs="+", required=True); a.add_argument("--m", type=int, nargs="+", default=[8])
    a.add_argument("--seeds", type=int, nargs="+", default=[1, 2, 3, 4, 5])
    a.add_argument("--dist", default="lognormal"); a.add_argument("--timeout", type=float, default=120)
    a.add_argument("--data-dir", help="reuse/create instances here (default: temp dir)")
    a.add_argument("--out", required=True); a.add_argument("--tol", type=float, default=1e-9)
    a.add_argument("--skip-after-timeout", action="store_true", help="skip a solver for larger N after it timed out")
    a = a.parse_args()
    solvers = [s.split("=", 1) for s in a.solver]
    tmp = tempfile.TemporaryDirectory(); dd = a.data_dir or tmp.name; os.makedirs(dd, exist_ok=True)
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    timed_out = set(); rows = []
    for n in sorted(a.n):
        for m in a.m:
            for seed in a.seeds:
                inp = f"{dd}/syn_n{n}_m{m}_s{seed}.csv"
                if not os.path.exists(inp): write(gen(n, m, seed, a.dist), inp)
                energies = {}
                for name, spec in solvers:
                    if a.skip_after_timeout and name in timed_out:
                        r = dict(status="skipped", time_ms="", mem_kb="", energy="", violations="")
                    else:
                        r = run_one(build_cmd(spec, inp, m), a.timeout)
                        if r["status"] == "timeout": timed_out.add(name)
                    if r["status"] == "ok": energies[name] = float(r["energy"])
                    rows.append(dict(n=n, m=m, seed=seed, solver=name, **r))
                # correctness cross-check: all solvers must agree on optimum
                if len(energies) > 1:
                    lo, hi = min(energies.values()), max(energies.values())
                    if hi - lo > a.tol * max(1.0, abs(hi)):
                        print(f"WARNING mismatch n={n} m={m} seed={seed}: {energies}", file=sys.stderr)
                print(f"n={n} m={m} seed={seed} done", file=sys.stderr)
    with open(a.out, "w", newline="") as f:
        wr = csv.DictWriter(f, fieldnames=["n", "m", "seed", "solver", "status", "time_ms", "mem_kb", "energy", "violations"])
        wr.writeheader(); wr.writerows(rows)
    print(f"wrote {a.out} ({len(rows)} rows)")

if __name__ == "__main__": main()
