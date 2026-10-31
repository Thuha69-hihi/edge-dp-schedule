# -*- coding: utf-8 -*-
"""Kiểm thử khác biệt cho các trường hợp biên.

Yêu cầu đề bài: mọi solver chính xác phải cho CÙNG kết quả (sai số energy
<= 1e-9), kể cả khi không có nghiệm (deadline rất chặt). Các trường hợp biên:
  * N = 1 (một tác vụ)
  * M = 1 (một node)
  * M >= N (đủ node, thậm chí dư)
  * workload = 0 (tác vụ không tốn năng lượng tính toán)
  * deadline rất chặt -> KHÔNG có nghiệm (cap = f_max*T hoặc link_cap = B*T nhỏ)
"""
import math

import pytest

from _harness import (EXACT_CUBIC, EXACT_QUADRATIC, cross_check, energies_close,
                      ensure_built, make_instance, run_solver)

@pytest.fixture(scope="module", autouse=True)
def _built():
    ensure_built()


# ---------------- N = 1 (một tác vụ) ----------------

def test_n1_m1_co_nghiem():
    """
    N=1, M=1: đoạn duy nhất k=0, d_0 không bị link_cap chặn.

    energy = E_on + k/T^2 * w^3 + eps*d_0 = 3 + 2*125 + 0.5*7 = 256.5
    """
    inst = make_instance(n=1, m=1, w=[5], d=[7], kappa=2.0, e_on=3.0,
                         eps=0.5, cap=10.0, link_cap=1.0)
    n_ok, n_inf = cross_check(EXACT_CUBIC, [inst], tag="n1_m1")
    assert (n_ok, n_inf) == (1, 0)
    for algo in EXACT_CUBIC:
        sol = run_solver(algo, inst)
        assert energies_close(sol["energy"], 256.5), algo
        assert sol["nodes_used"] == 1
        assert sol["cuts"] == []
        assert sol["segments"][0]["work"] == 5


def test_n1_m1_vo_nghiem_deadline_chat():
    """
    N=1, M=1 nhưng w_1 > cap: KHÔNG có nghiệm.
    Mọi solver phải thống nhất: feasible=false, energy=null, nodes_used=0.
    """
    inst = make_instance(n=1, m=1, w=[5], d=[7], kappa=2.0, e_on=3.0,
                         eps=0.5, cap=4.0, link_cap=1.0)  # cap=4 < w=5
    n_ok, n_inf = cross_check(EXACT_CUBIC, [inst], tag="n1_m1_vo_nghiem")
    assert (n_ok, n_inf) == (0, 1)
    for algo in EXACT_CUBIC:
        sol = run_solver(algo, inst)
        assert sol["feasible"] is False
        assert sol["energy"] is None
        assert sol["nodes_used"] == 0


def test_n1_voi_m_du_node():
    """N=1, M=10: chỉ dùng đúng 1 node dù M dư nhiều."""
    inst = make_instance(n=1, m=10, w=[5], d=[7], kappa=2.0, e_on=3.0,
                         eps=0.5, cap=10.0, link_cap=1.0)
    cross_check(EXACT_CUBIC, [inst], tag="n1_m10")
    for algo in EXACT_CUBIC:
        sol = run_solver(algo, inst)
        assert sol["feasible"] is True
        assert sol["nodes_used"] == 1


# ---------------- M = 1 (một node) ----------------

def test_m1_kha_dung():
    """M=1, tổng khối lượng vừa cap: một đoạn duy nhất.

    energy = E_on + k*S_N^3 + eps*d_0 = 250 + 1000 + 3 = 1253.
    (Với M=1 không có cắt trong nên link_cap không ảnh hưởng.)"""
    inst = make_instance(n=4, m=1, w=[2, 3, 1, 4], d=[3, 4, 2, 6],
                         kappa=1.0, e_on=250.0, eps=1.0, cap=12.0, link_cap=5.0)
    n_ok, n_inf = cross_check(EXACT_CUBIC, [inst], tag="m1_ok")
    assert (n_ok, n_inf) == (1, 0)
    for algo in EXACT_CUBIC:
        sol = run_solver(algo, inst)
        assert energies_close(sol["energy"], 1253.0)
        assert sol["nodes_used"] == 1


def test_m1_vo_nghiem_vi_cap():
    """M=1, tổng khối lượng 12 > cap=8: KHÔNG có nghiệm (deadline chặt)."""
    inst = make_instance(n=4, m=1, w=[2, 3, 1, 4], d=[3, 4, 2, 6],
                         kappa=1.0, e_on=250.0, eps=1.0, cap=8.0, link_cap=5.0)
    n_ok, n_inf = cross_check(EXACT_CUBIC, [inst], tag="m1_inf")
    assert (n_ok, n_inf) == (0, 1)
    for algo in EXACT_CUBIC:
        sol = run_solver(algo, inst)
        assert sol["feasible"] is False and sol["energy"] is None


# ---------------- M >= N (đủ node) ----------------

def _small():
    return dict(w=[2, 3, 1, 4], d=[3, 4, 2, 6], kappa=1.0, e_on=250.0,
                eps=1.0, cap=8.0, link_cap=5.0)


def test_m_bang_n():
    """M = N = 4: mỗi tác vụ một node là khả thi, cuts nằm trong d <= link_cap."""
    inst = make_instance(n=4, m=4, **_small())
    cross_check(EXACT_CUBIC, [inst], tag="m_eq_n")
    for algo in EXACT_CUBIC:
        sol = run_solver(algo, inst)
        assert sol["feasible"] is True
        assert 1 <= sol["nodes_used"] <= 4


def test_m_lon_hon_n():
    """M = 10 > N = 4: kết quả phải giống M = 4 (node dư không làm thay đổi energy)."""
    base = _small()
    sols = {}
    for m in (4, 10):
        inst = make_instance(n=4, m=m, **base)
        for algo in EXACT_CUBIC:
            sols[(m, algo)] = run_solver(algo, inst)
        cross_check(EXACT_CUBIC, [inst], tag=f"m_gt_n_m{m}")
    for algo in EXACT_CUBIC:
        a, b = sols[(4, algo)], sols[(10, algo)]
        assert a["feasible"] and b["feasible"]
        assert energies_close(a["energy"], b["energy"]), algo
        assert a["nodes_used"] == b["nodes_used"], algo


def test_m_lon_eon_0_dung_du_node():
    """E_on = 0: tách nhỏ đoạn luôn không tốn thêm (cubic subadditive) và tiết kiệm
    eps*d_k, nên nghiệm tối ưu dùng đủ N node. Kiểm tra công thức đóng:
    energy = k*SUM(w^3) + eps*SUM(d) = (8+27+1+64) + (3+4+2+6) = 115."""
    inst = make_instance(n=4, m=10, w=[2, 3, 1, 4], d=[3, 4, 2, 6],
                         kappa=1.0, e_on=0.0, eps=1.0, cap=1e9, link_cap=1e9)
    cross_check(EXACT_CUBIC, [inst], tag="eon0")
    for algo in EXACT_CUBIC:
        sol = run_solver(algo, inst)
        assert energies_close(sol["energy"], 115.0), algo
        assert sol["nodes_used"] == 4, algo


# ---------------- workload = 0 ----------------

def test_workload_0():
    """Mọi w_i = 0 (giống test zero_work_tasks của test_main.cpp):
    nghiệm tối ưu là 1 đoạn, energy = E_on = 5."""
    inst = make_instance(n=3, m=2, w=[0, 0, 0], d=[0, 0, 0],
                         kappa=1.0, e_on=5.0, eps=1.0, cap=1.0, link_cap=1.0)
    cross_check(EXACT_CUBIC, [inst], tag="w0")
    for algo in EXACT_CUBIC:
        sol = run_solver(algo, inst)
        assert energies_close(sol["energy"], 5.0), algo
        assert sol["nodes_used"] == 1, algo


def test_workload_0_voi_cap_0():
    """Biên tuyệt đối: cap = 0 nhưng mọi w_i = 0 (khối lượng đoạn 0 không vượt cap 0)."""
    inst = make_instance(n=1, m=1, w=[0], d=[2], kappa=1.0, e_on=5.0,
                         eps=1.0, cap=0.0, link_cap=1e9)
    cross_check(EXACT_CUBIC, [inst], tag="w0_cap0")
    for algo in EXACT_CUBIC:
        sol = run_solver(algo, inst)
        assert sol["feasible"] is True, algo
        assert energies_close(sol["energy"], 7.0), algo  # 5 + 0 + 1*2


def test_workload_0_tron_voi_task_thuong():
    """w có cả 0 lẫn dương, các đoạn nhiều khả năng hòa chi phí -> so energy."""
    inst = make_instance(n=4, m=3, w=[0, 3, 0, 2], d=[1, 2, 1, 2],
                         kappa=1.0, e_on=10.0, eps=1.0, cap=5.0, link_cap=2.0)
    cross_check(EXACT_CUBIC, [inst], tag="w0_mixed")


# ---------------- deadline rất chặt: KHÔNG có nghiệm ----------------

def test_deadline_chat_khong_co_nghiem_roi_du_node_thi_co():
    """w=[2,3,6,1], cap=6.5: tác vụ 6 phải chạy một mình, cần 3 node.
    M=2 -> vô nghiệm; M=3 -> khả thi. Mọi solver phải thống nhất cả hai."""
    kw = dict(kappa=1.0, e_on=250.0, eps=1.0, cap=6.5, link_cap=1e9)
    inf_inst = make_instance(n=4, m=2, w=[2, 3, 6, 1], d=[1, 1, 1, 1], **kw)
    ok_inst = make_instance(n=4, m=3, w=[2, 3, 6, 1], d=[1, 1, 1, 1], **kw)
    assert cross_check(EXACT_CUBIC, [inf_inst], tag="deadline_M2") == (0, 1)
    assert cross_check(EXACT_CUBIC, [ok_inst], tag="deadline_M3") == (1, 0)
    for algo in EXACT_CUBIC:
        assert run_solver(algo, inf_inst)["feasible"] is False
        sol = run_solver(algo, ok_inst)
        assert sol["feasible"] is True and sol["nodes_used"] == 3


def test_deadline_chat_tung_doan():
    """w=[5,5,5], cap=6: ghép hai tác vụ nào cũng 10 > 6.
    M=2 -> vô nghiệm; M=3 -> khả thi (mỗi tác vụ một node)."""
    kw = dict(kappa=1.0, e_on=1.0, eps=0.1, cap=6.0, link_cap=1e9)
    inf_inst = make_instance(n=3, m=2, w=[5, 5, 5], d=[1, 1, 1], **kw)
    ok_inst = make_instance(n=3, m=3, w=[5, 5, 5], d=[1, 1, 1], **kw)
    assert cross_check(EXACT_CUBIC, [inf_inst], tag="deadline_tungdoan_M2") == (0, 1)
    assert cross_check(EXACT_CUBIC, [ok_inst], tag="deadline_tungdoan_M3") == (1, 0)


def test_deadline_chat_link_cap_bat_ca_cai_cat():
    """link_cap=5 mà mọi d_k (k>=1) đều 9 > 5: cấm toàn bộ điểm cắt trong.
    N=4, cap=12: buộc chạy 1 node (10 <= 12) -> khả thi.
    energy = E_on + S_N^3 + eps*d_0 = 250 + 1000 + 3 = 1253 (d_0 = 3, không phải 9).
    N=4, cap=9:  1 node cũng vượt cap -> vô nghiệm tuyệt đối."""
    w, d = [2, 3, 1, 4], [3, 9, 9, 9]
    ok_inst = make_instance(n=4, m=4, w=w, d=d, kappa=1.0, e_on=250.0,
                            eps=1.0, cap=12.0, link_cap=5.0)
    inf_inst = make_instance(n=4, m=4, w=w, d=d, kappa=1.0, e_on=250.0,
                             eps=1.0, cap=9.0, link_cap=5.0)
    assert cross_check(EXACT_CUBIC, [ok_inst], tag="linkcap_ok") == (1, 0)
    assert cross_check(EXACT_CUBIC, [inf_inst], tag="linkcap_inf") == (0, 1)
    for algo in EXACT_CUBIC:
        sol = run_solver(algo, ok_inst)
        assert sol["nodes_used"] == 1 and energies_close(sol["energy"], 1253.0), algo
        assert run_solver(algo, inf_inst)["feasible"] is False


def test_vo_nghiem_bat_ke_so_node():
    """Một tác vụ nặng hơn cap: dù M = 100 vẫn KHÔNG có nghiệm."""
    inst = make_instance(n=1, m=100, w=[5], d=[1], kappa=1.0, e_on=1.0,
                         eps=1.0, cap=4.0, link_cap=1e9)
    assert cross_check(EXACT_CUBIC, [inst], tag="inf_M100") == (0, 1)


# ---------------- Biến thể bậc hai (model quadratic): thêm solver CHT ----------------

def test_quadratic_cac_bien_ben():
    """Cùng bộ edge case nhưng model='quadratic' -> bộ so sánh gồm cả CHT."""
    cases = [
        # N=1, M=1 khả thi: 3 + 2*25 + 1*5 + 0.5*7 = 61.5
        make_instance(n=1, m=1, w=[5], d=[7], kappa=2.0, e_on=3.0, eps=0.5,
                      cap=10.0, link_cap=1.0, model="quadratic", quad_a=2.0, quad_b=1.0),
        # N=1, M=1 vô nghiệm (cap < w)
        make_instance(n=1, m=1, w=[5], d=[7], kappa=2.0, e_on=3.0, eps=0.5,
                      cap=4.0, link_cap=1.0, model="quadratic", quad_a=2.0, quad_b=1.0),
        # M >= N, workload 0
        make_instance(n=3, m=5, w=[0, 0, 0], d=[1, 0, 1], kappa=1.0, e_on=5.0,
                      eps=1.0, cap=1.0, link_cap=1.0, model="quadratic", quad_a=1.0, quad_b=0.0),
        # deadline chặt, không có nghiệm
        make_instance(n=4, m=2, w=[2, 3, 6, 1], d=[1, 1, 1, 1], kappa=1.0, e_on=250.0,
                      eps=1.0, cap=6.5, link_cap=1e9, model="quadratic", quad_a=0.5, quad_b=2.0),
        # M=1 khả thi: S_N = 10 -> 250 + 0.5*100 + 2*10 + 1*3 = 323
        make_instance(n=4, m=1, w=[2, 3, 1, 4], d=[3, 4, 2, 6], kappa=1.0, e_on=250.0,
                      eps=1.0, cap=12.0, link_cap=5.0, model="quadratic", quad_a=0.5, quad_b=2.0),
    ]
    n_ok, n_inf = cross_check(EXACT_QUADRATIC, cases, tag="quad_edge")
    assert n_ok == 3 and n_inf == 2
    # Giá trị đóng: bắt cả lỗi chung của CostModel mà cross-check giữa các solver không thấy.
    expected = {0: 61.5, 4: 323.0}
    for idx, e in expected.items():
        for algo in EXACT_QUADRATIC:
            got = run_solver(algo, cases[idx])["energy"]
            assert energies_close(got, e), f"case {idx} {algo}: {got} != {e}"


# ---------------- Cấu trúc đầu ra ----------------

def test_dang_dau_ra_json():
    """Đầu ra JSON phải đủ trường theo README và khớp bảng số liệu mục 6."""
    inst = make_instance(n=5, m=3, w=[2, 3, 1, 4, 2], d=[3, 4, 2, 6, 1],
                         kappa=1.0, e_on=250.0, eps=1.0, cap=8.0, link_cap=5.0)
    sol = run_solver("baseline", inst)
    for key in ("feasible", "energy", "nodes_used", "cuts", "segments",
                "solver", "time_ms"):
        assert key in sol, key
    assert sol["solver"] == "baseline"
    assert sol["time_ms"] >= 0
    assert sol["cuts"] == [2] and sol["nodes_used"] == 2
    assert [(s["from"], s["to"], s["work"]) for s in sol["segments"]] == [(1, 2, 5), (3, 5, 7)]
    assert math.isclose(sol["segments"][0]["cost"], 378, rel_tol=1e-9, abs_tol=1e-9)
    assert math.isclose(sol["segments"][1]["cost"], 595, rel_tol=1e-9, abs_tol=1e-9)
