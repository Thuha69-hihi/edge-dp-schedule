#!/usr/bin/env python3
"""Tasks 5.02/6.02 helper: mean +- std per (solver, N, M) and log-log slope of time vs N."""
import csv, sys, math, collections
rows = [r for r in csv.DictReader(open(sys.argv[1])) if r["status"] == "ok"]
g = collections.defaultdict(list)
for r in rows: g[(r["solver"], int(r["n"]), int(r["m"]))].append((float(r["time_ms"]), float(r["mem_kb"])))
def ms(v): mu = sum(v)/len(v); return mu, (sum((x-mu)**2 for x in v)/max(1,len(v)-1))**.5
print("solver,n,m,runs,time_ms_mean,time_ms_std,mem_kb_mean")
for k in sorted(g):
    t, mm = ms([x[0] for x in g[k]]), ms([x[1] for x in g[k]])
    print(f"{k[0]},{k[1]},{k[2]},{len(g[k])},{t[0]:.3f},{t[1]:.3f},{mm[0]:.0f}")
print("\n# log-log slope of time vs N (needs >=2 N values), per solver and M")
by = collections.defaultdict(dict)
for (s, n, m), v in g.items(): by[(s, m)][n] = ms([x[0] for x in v])[0]
for (s, m), d in sorted(by.items()):
    pts = [(math.log(n), math.log(t)) for n, t in sorted(d.items()) if t > 0]
    if len(pts) >= 2:
        mx = sum(p[0] for p in pts)/len(pts); my = sum(p[1] for p in pts)/len(pts)
        sl = sum((x-mx)*(y-my) for x, y in pts)/sum((x-mx)**2 for x, _ in pts)
        print(f"{s},M={m},slope={sl:.2f}")
