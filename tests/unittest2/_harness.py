"""
Dụng cụ chung cho khung kiểm thử khác biệt (differential testing).

Mỗi instance JSON được chạy qua TẤT CẢ các solver chính xác bằng CLI
`./build/edgedp --algo <tên> --input <file>`; sau đó đối chiếu:
  * kết luận khả thi / vô nghiệm phải giống nhau,
  * `energy` phải khớp với sai số <= 1e-9 (tuyệt đối và tương đối).

Biểu diễn "không có nghiệm" trong project: Solution.feasible = false,
`energy` = null trong JSON đầu ra, nodes_used = 0 (CLI vẫn exit 0).
Solver chưa cài đặt ném NotImplemented -> CLI exit 3 (test sẽ SKIP, không
phải là vô nghiệm).
"""
import json
import math
import os
import subprocess
import tempfile

TEST_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(TEST_DIR, "..", ".."))
EDGEDP = os.path.join(ROOT, "build", "edgedp")

# Sai số cho phép khi so energy (README + docs/model.md: sai số tương đối <= 1e-9;
# năng lượng có thể tới ~1e20 nên dùng cả abs_tol và rel_tol như isclose).
TOL = 1e-9

# Các solver chính xác:
#   - Trên mô hình bậc BA (cubic, mặc định): baseline, dc, bruteforce.
#     (cht không áp dụng được cho bậc ba - số hạng trộn S_i^2*S_k - sẽ ném NotImplemented.)
#   - Trên biến thể BẬC HAI (quadratic): thêm cht vào bộ so sánh.
EXACT_CUBIC = ["baseline", "dc", "bruteforce"]
EXACT_QUADRATIC = ["baseline", "dc", "cht", "bruteforce"]


def ensure_built():
    """Biên dịch CLI nếu chưa có binary (gọi make, dùng Makefile của project)."""
    if os.path.exists(EDGEDP):
        return
    subprocess.run(["make", "build/edgedp"], cwd=ROOT, check=True)


def run_solver(algo, instance):
    """Chạy một solver qua CLI trên `instance` (dict), trả về dict kết quả JSON."""
    fd, path = tempfile.mkstemp(suffix=".json")
    try:
        with os.fdopen(fd, "w") as f:
            json.dump(instance, f)
        proc = subprocess.run(
            [EDGEDP, "--algo", algo, "--input", path],
            capture_output=True, text=True, timeout=600,
        )
    finally:
        os.unlink(path)
    assert proc.returncode == 0, (
        f"[{algo}] exit={proc.returncode} stderr={proc.stderr.strip()[:400]}")
    assert proc.stdout.strip(), f"[{algo}] khong co output: {proc.stderr[:400]}"
    return json.loads(proc.stdout)


def make_instance(n, m, w, d, kappa=1.0, e_on=250.0, eps=1.0, cap=8.0,
                  link_cap=5.0, model=None, **extra):
    """Dựng instance JSON đúng định dạng input của CLI (xem README)."""
    params = {
        "kappa_over_T2": kappa, "e_on": e_on, "eps": eps,
        "cap": cap, "link_cap": link_cap,
    }
    if model is not None:
        params["model"] = model
    inst = {"n": n, "m": m, "w": list(w), "d": list(d), "params": params}
    inst.update(extra)
    return inst


def energies_close(a, b):
    """So hai năng lượng: sai số tuyệt đối hoặc tương đối <= 1e-9."""
    return math.isclose(a, b, rel_tol=TOL, abs_tol=TOL)


def cross_check(algos, instances, tag=""):
    """Chạy tất cả solver trên từng instance và yêu cầu cùng kết quả.

    Trả về (số trường hợp khả thi, số trường hợp vô nghiệm) để test
    kiểm tra độ phủ của bộ dữ liệu ngẫu nhiên.
    """
    n_feasible = n_infeasible = 0
    for idx, inst in enumerate(instances):
        sols = {a: run_solver(a, inst) for a in algos}
        feas = {a: s["feasible"] for a, s in sols.items()}
        assert len(set(feas.values())) == 1, (
            f"[{tag} #{idx}] feasible khong khop: {feas}\n"
            f"instance={json.dumps(inst)[:500]}")
        if feas[algos[0]]:
            n_feasible += 1
            ref = sols[algos[0]]["energy"]
            for a in algos[1:]:
                got = sols[a]["energy"]
                assert energies_close(got, ref), (
                    f"[{tag} #{idx}] {a} energy={got} != {algos[0]} energy={ref}\n"
                    f"instance={json.dumps(inst)[:500]}")
        else:
            n_infeasible += 1
    return n_feasible, n_infeasible
