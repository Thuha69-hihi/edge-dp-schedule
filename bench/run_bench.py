#!/usr/bin/env python3
"""bench/run_bench.py - Benchmark harness (việc 3.06, TV5 Thành Công).

Chạy các solver C++ trên nhiều instance (nhiều seed) và xuất CSV:
  * thời gian: time_ms do chính solver đo bằng std::chrono (chỉ phần giải, không tính đọc
    file) + wall_ms do harness đo (cả tiến trình);
  * bộ nhớ đỉnh (RSS): ưu tiên `/usr/bin/time -v` (cột mem_src = time), nếu không có thì đọc
    VmHWM của tiến trình con trong /proc (mem_src = proc, Linux). KHÔNG dùng ru_maxrss của
    os.wait4/RUSAGE_CHILDREN: sau fork() tiến trình con thừa hưởng RSS của Python cha nên
    giá trị luôn >= RSS của Python (~12 MB) và che mất các solver nhẹ; RUSAGE_CHILDREN còn lấy
    giá trị lớn nhất cộng dồn qua mọi tiến trình con. Chế độ proc là lấy mẫu định kỳ (mỗi
    <= 2 ms) nên chỉ gần đúng với lần chạy ngắn hơn ~10 ms; dùng /usr/bin/time cho số liệu chính thức;
  * đối chiếu: solver "chính xác" (baseline, dnc, cht) phải cho cùng năng lượng (sai số
    tương đối <= 1e-9) -> cột match_ref; heuristic có thêm energy_saved_pct so với năng lượng
    tối ưu (chỉ tính khi heuristic khả thi).

Hợp đồng với CLI (việc 3.04): mỗi lần gọi
    <bin> --solver NAME --input FILE.json
in ra stdout một object JSON có các trường: solver, feasible, energy (null nếu vô nghiệm),
nodes_used, time_ms; heuristic có thêm violated_cap, violated_link. NAME = "load" (tùy chọn,
dùng với --load-solver) chỉ nạp file rồi thoát, để trừ bộ nhớ nền của việc đọc input.

Ví dụ:
    python bench/run_bench.py --bin build/edgedp_cli data/synthetic --repeats 3 \\
        --max-n baseline=10000 --timeout 600 --out results/time_mem.csv \\
        --summary results/time_mem_summary.csv
    python bench/run_bench.py --bin build/edgedp_cli data/small/manifest.csv --solvers baseline,dnc

Đầu vào (inputs): file .json, thư mục (lấy mọi *.json), hoặc manifest.csv do gen_data.py sinh.
Chỉ dùng thư viện chuẩn (Python >= 3.8). Đo bộ nhớ cần Linux (trên Ubuntu: `sudo apt install
time` nếu thiếu /usr/bin/time); macOS/Windows: cột bộ nhớ để trống.
"""
import argparse
import csv
import glob
import json
import os
import statistics
import subprocess
import sys
import tempfile
import time

DEFAULT_SOLVERS = "baseline,dnc,cht,greedy,equal,single,roundrobin"
DEFAULT_EXACT = "baseline,dnc,cht"
HEURISTICS = {"greedy", "equal", "single", "roundrobin"}
REL_TOL = 1e-9

FIELDS = [
    "file", "source", "n", "m", "alpha", "beta", "seed", "solver", "rep", "status",
    "feasible", "energy", "nodes_used", "time_ms", "wall_ms",
    "peak_rss_mb", "load_rss_mb", "net_rss_mb", "mem_src",
    "violated_cap", "violated_link", "match_ref", "energy_saved_pct", "note",
]


# ----------------------------------------------------------------------------- chạy 1 lần
TIME_BIN = "/usr/bin/time"
_mem_mode = None


def detect_mem_mode(requested):
    """auto: time nếu /usr/bin/time -v dùng được, ngược lại proc nếu có /proc, ngược lại none."""
    global _mem_mode
    if requested != "auto":
        _mem_mode = requested
        return _mem_mode
    _mem_mode = "none"
    if os.path.exists(TIME_BIN):
        try:
            r = subprocess.run([TIME_BIN, "-v", sys.executable, "-c", "pass"],
                               capture_output=True, timeout=30)
            if b"Maximum resident set size" in r.stderr:
                _mem_mode = "time"
                return _mem_mode
        except (OSError, subprocess.SubprocessError):
            pass
    if os.path.isdir("/proc/self"):
        _mem_mode = "proc"
    return _mem_mode


def _read_cmdline(pid):
    try:
        with open("/proc/%s/cmdline" % pid, "rb") as f:
            return f.read()
    except OSError:
        return None


def _vmhwm_mb(pid, self_cmdline):
    """VmHWM (MB) của tiến trình pid, chỉ khi nó đã exec sang chương trình đích."""
    if _read_cmdline(pid) == self_cmdline:
        return None  # còn là bản sao của Python giữa fork() và exec(): RSS chưa phản ánh solver
    try:
        with open("/proc/%d/status" % pid, encoding="ascii", errors="replace") as f:
            for line in f:
                if line.startswith("VmHWM:"):
                    return int(line.split()[1]) / 1024.0
    except (OSError, ValueError):
        pass
    return None


def run_once(cmd, timeout):
    """Chạy một tiến trình con. Trả về dict: status, stdout, stderr, wall_ms, rss_mb, rc."""
    mode = _mem_mode or "none"
    full = [TIME_BIN, "-v"] + cmd if mode == "time" else cmd
    self_cmdline = _read_cmdline("self") if mode == "proc" else None
    with tempfile.TemporaryFile() as fout, tempfile.TemporaryFile() as ferr:
        t0 = time.perf_counter()
        p = subprocess.Popen(full, stdout=fout, stderr=ferr, start_new_session=(os.name == "posix"))
        status, rss = "ok", None
        delay = 0.0005
        while True:
            if mode == "proc":
                v = _vmhwm_mb(p.pid, self_cmdline)
                if v is not None:
                    rss = v  # VmHWM chỉ tăng: lấy lần đọc cuối
            if p.poll() is not None:
                break
            if timeout and time.perf_counter() - t0 > timeout:
                status = "timeout"
                try:
                    os.killpg(p.pid, 9) if os.name == "posix" else p.kill()
                except OSError:
                    p.kill()
                p.wait()
                break
            time.sleep(delay)
            delay = min(delay * 2, 0.002 if mode == "proc" else 0.02)
        wall_ms = (time.perf_counter() - t0) * 1000.0
        fout.seek(0)
        ferr.seek(0)
        out = fout.read().decode("utf-8", "replace")
        err = ferr.read().decode("utf-8", "replace")
    if mode == "time" and status == "ok":
        for line in err.splitlines():
            if "Maximum resident set size" in line:
                rss = int(line.rsplit(":", 1)[1]) / 1024.0
                break
    return {"status": status, "stdout": out, "stderr": err, "wall_ms": wall_ms,
            "rss_mb": rss, "rc": p.returncode if status == "ok" else None}


def parse_result(text):
    text = text.strip()
    if not text:
        return None
    try:
        return json.loads(text)
    except ValueError:
        try:
            return json.loads(text.splitlines()[-1])
        except ValueError:
            return None


# ----------------------------------------------------------------------------- đầu vào
def collect_inputs(paths):
    files = []
    for p in paths:
        if os.path.isdir(p):
            files += sorted(glob.glob(os.path.join(p, "**", "*.json"), recursive=True))
        elif p.lower().endswith(".csv"):
            base = os.path.dirname(p)
            with open(p, newline="", encoding="utf-8") as f:
                for row in csv.DictReader(f):
                    files.append(os.path.join(base, row["file"]))
        elif any(ch in p for ch in "*?["):
            files += sorted(glob.glob(p))
        else:
            files.append(p)
    seen, out = set(), []
    for f in files:
        if f not in seen:
            seen.add(f)
            out.append(f)
    return out


def read_header(path):
    with open(path, encoding="utf-8") as f:
        inst = json.load(f)
    meta = inst.get("meta", {})
    return {
        "n": inst.get("n"), "m": inst.get("m"), "seed": inst.get("seed"),
        "source": inst.get("source", ""),
        "alpha": meta.get("alpha", ""), "beta": meta.get("beta", ""),
    }


def parse_max_n(items):
    res = {}
    for it in items or []:
        name, _, val = it.partition("=")
        res[name] = int(float(val))
    return res


# ----------------------------------------------------------------------------- chạy 1 file
def bench_file(args, path, solvers, exact, max_n):
    head = read_header(path)
    base = {"file": os.path.basename(path), **head}
    load_rss = None
    if args.load_solver:
        r = run_once([args.bin, "--solver", args.load_solver, "--input", path], args.timeout)
        if r["status"] == "ok" and r["rc"] == 0:
            load_rss = r["rss_mb"]

    rows = []
    for solver in solvers:
        for rep in range(1, args.repeats + 1):
            row = dict(base, solver=solver, rep=rep, note="")
            lim = max_n.get(solver)
            if lim is not None and head["n"] is not None and head["n"] > lim:
                row["status"] = "skipped"
                row["note"] = "n > max-n"
                rows.append(row)
                break  # bỏ qua mọi lần lặp
            r = run_once([args.bin, "--solver", solver, "--input", path], args.timeout)
            row["wall_ms"] = round(r["wall_ms"], 3)
            row["peak_rss_mb"] = "" if r["rss_mb"] is None else round(r["rss_mb"], 3)
            row["mem_src"] = _mem_mode if r["rss_mb"] is not None else ""
            if r["rss_mb"] is not None and load_rss is not None:
                row["load_rss_mb"] = round(load_rss, 3)
                row["net_rss_mb"] = round(max(0.0, r["rss_mb"] - load_rss), 3)
            row["status"] = r["status"]
            if r["status"] == "ok":
                res = parse_result(r["stdout"])
                if r["rc"] != 0 or res is None:
                    row["status"] = "error"
                    row["note"] = ("rc=%s; " % r["rc"] + r["stderr"].strip()[:200]).strip()
                else:
                    row["feasible"] = res.get("feasible")
                    row["energy"] = res.get("energy")
                    row["nodes_used"] = res.get("nodes_used")
                    row["time_ms"] = res.get("time_ms")
                    row["violated_cap"] = res.get("violated_cap", "")
                    row["violated_link"] = res.get("violated_link", "")
            rows.append(row)
            if r["status"] != "ok":
                break  # timeout: không lặp lại
    annotate(rows, exact)
    return rows


def annotate(rows, exact):
    """Điền match_ref (solver chính xác phải cùng năng lượng) và energy_saved_pct (heuristic)."""
    ok_exact = [r for r in rows if r["solver"] in exact and r["status"] == "ok"]
    ref = None  # (feasible, energy)
    for r in ok_exact:
        if r.get("feasible") is True and r.get("energy") is not None:
            ref = (True, float(r["energy"]))
            break
    if ref is None and ok_exact:
        ref = (False, None)
    for r in ok_exact:
        if ref is None:
            break
        if ref[0]:
            same = (r.get("feasible") is True and r.get("energy") is not None and
                    abs(float(r["energy"]) - ref[1]) <= REL_TOL * max(1.0, abs(ref[1])))
        else:
            same = r.get("feasible") is False
        r["match_ref"] = "ok" if same else "MISMATCH"
        if not same:
            print("CẢNH BÁO: %s: %s lệch năng lượng so với solver chính xác khác" %
                  (r["file"], r["solver"]), file=sys.stderr)
    if ref is not None and ref[0]:
        for r in rows:
            if (r["solver"] in HEURISTICS and r["status"] == "ok" and r.get("feasible") is True
                    and r.get("energy")):
                e = float(r["energy"])
                r["energy_saved_pct"] = round((e - ref[1]) / e * 100.0, 6)


# ----------------------------------------------------------------------------- tổng hợp
def _ms(vals):
    vals = [v for v in vals if v is not None]
    if not vals:
        return "", ""
    return round(statistics.mean(vals), 6), round(statistics.stdev(vals), 6) if len(vals) > 1 else 0.0


def _num(x):
    try:
        return None if x in ("", None) else float(x)
    except (TypeError, ValueError):
        return None


def write_summary(rows, path):
    """Trung bình ± độ lệch chuẩn theo (solver, n, m, alpha, beta): gộp các lần lặp của mỗi
    file trước (trung bình), rồi tính trên các seed."""
    per_file = {}
    for r in rows:
        per_file.setdefault((r["solver"], r["file"]), []).append(r)
    groups = {}
    for (solver, _), rs in per_file.items():
        key = (solver, rs[0]["n"], rs[0]["m"], rs[0]["alpha"], rs[0]["beta"])
        groups.setdefault(key, []).append(rs)

    def avg(rs, col):
        v = [_num(r.get(col)) for r in rs if r["status"] == "ok"]
        v = [x for x in v if x is not None]
        return statistics.mean(v) if v else None

    fields = ["solver", "n", "m", "alpha", "beta", "files", "ok", "timeout", "skipped", "error",
              "time_ms_mean", "time_ms_std", "wall_ms_mean", "wall_ms_std",
              "peak_rss_mb_mean", "peak_rss_mb_std", "net_rss_mb_mean", "net_rss_mb_std",
              "energy_mean", "energy_std", "violation_rate", "energy_saved_pct_mean",
              "energy_saved_pct_std", "mismatch"]
    with open(path, "w", newline="", encoding="utf-8") as f:
        wr = csv.DictWriter(f, fieldnames=fields, lineterminator="\n")
        wr.writeheader()
        for key in sorted(groups, key=lambda k: [str(x) for x in k]):
            solver, n, m, alpha, beta = key
            files = groups[key]
            st = [fs[0]["status"] for fs in files]
            row = {"solver": solver, "n": n, "m": m, "alpha": alpha, "beta": beta,
                   "files": len(files), "ok": st.count("ok"), "timeout": st.count("timeout"),
                   "skipped": st.count("skipped"), "error": st.count("error")}
            for col, name in (("time_ms", "time_ms"), ("wall_ms", "wall_ms"),
                              ("peak_rss_mb", "peak_rss_mb"), ("net_rss_mb", "net_rss_mb"),
                              ("energy", "energy"), ("energy_saved_pct", "energy_saved_pct")):
                mean, std = _ms([avg(fs, col) for fs in files])
                row[name + "_mean"], row[name + "_std"] = mean, std
            feas = [fs[0].get("feasible") for fs in files if fs[0]["status"] == "ok"
                    and fs[0].get("feasible") is not None]
            row["violation_rate"] = round(1.0 - sum(1 for x in feas if x is True) / len(feas), 6) if feas else ""
            row["mismatch"] = sum(1 for fs in files for r in fs if r.get("match_ref") == "MISMATCH")
            wr.writerow(row)


# ----------------------------------------------------------------------------- main
def main(argv=None):
    ap = argparse.ArgumentParser(description="Benchmark harness cho các solver DP (việc 3.06).")
    ap.add_argument("inputs", nargs="+", help="file .json, thư mục, glob hoặc manifest.csv")
    ap.add_argument("--bin", required=True, help="đường dẫn CLI solver, vd build/edgedp_cli")
    ap.add_argument("--solvers", default=DEFAULT_SOLVERS, help="danh sách solver, cách nhau bằng dấu phẩy")
    ap.add_argument("--exact", default=DEFAULT_EXACT, help="solver phải cho cùng năng lượng tối ưu")
    ap.add_argument("--repeats", type=int, default=1, help="số lần lặp mỗi (solver, file)")
    ap.add_argument("--timeout", type=float, default=600.0, help="giây mỗi lần chạy; 0 = không giới hạn")
    ap.add_argument("--max-n", action="append", metavar="SOLVER=N",
                    help="bỏ qua solver khi n > N (vd baseline=10000); dùng nhiều lần được")
    ap.add_argument("--load-solver", default=None, metavar="NAME",
                    help="solver chỉ nạp file (vd 'load') để trừ bộ nhớ nền -> cột net_rss_mb")
    ap.add_argument("--mem-mode", choices=["auto", "time", "proc", "none"], default="auto",
                    help="cách đo bộ nhớ đỉnh (mặc định auto)")
    ap.add_argument("--out", default="results/time_mem.csv")
    ap.add_argument("--summary", default=None, help="CSV tổng hợp mean/std theo (solver, n, m, alpha, beta)")
    args = ap.parse_args(argv)

    if not os.path.isfile(args.bin):
        ap.error("không thấy CLI: %s (build trước, hoặc sửa --bin)" % args.bin)
    print("Đo bộ nhớ bằng: %s" % detect_mem_mode(args.mem_mode), file=sys.stderr)
    files = collect_inputs(args.inputs)
    if not files:
        ap.error("không có file input nào")
    solvers = [s for s in args.solvers.split(",") if s]
    exact = set(s for s in args.exact.split(",") if s)
    max_n = parse_max_n(args.max_n)

    out_dir = os.path.dirname(args.out)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    all_rows = []
    with open(args.out, "w", newline="", encoding="utf-8") as f:
        wr = csv.DictWriter(f, fieldnames=FIELDS, extrasaction="ignore", lineterminator="\n")
        wr.writeheader()
        for idx, path in enumerate(files, 1):
            print("[%d/%d] %s" % (idx, len(files), os.path.basename(path)), file=sys.stderr)
            rows = bench_file(args, path, solvers, exact, max_n)
            for r in rows:
                wr.writerow(r)
            f.flush()  # giữ kết quả dở dang nếu bị ngắt giữa chừng
            all_rows += rows
    print("Đã ghi %d dòng -> %s" % (len(all_rows), args.out), file=sys.stderr)
    if args.summary:
        sd = os.path.dirname(args.summary)
        if sd:
            os.makedirs(sd, exist_ok=True)
        write_summary(all_rows, args.summary)
        print("Đã ghi tổng hợp -> %s" % args.summary, file=sys.stderr)
    n_bad = sum(1 for r in all_rows if r.get("match_ref") == "MISMATCH")
    if n_bad:
        print("CÓ %d dòng MISMATCH: các solver chính xác không cho cùng năng lượng." % n_bad, file=sys.stderr)
        return 3
    return 0


if __name__ == "__main__":
    sys.exit(main())
