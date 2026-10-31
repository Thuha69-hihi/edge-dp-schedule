# -*- coding: utf-8 -*-
"""Test hồi quy CỦA CHÍNH BỘ KIỂM THỨ (canary).

Chứng minh cổng so sánh energy sai số 1e-9 của cross_check() thực sự hoạt động:
  * một solver "bị cài lỗi" lệch energy > 1e-9  -> phải bị BẮT (AssertionError)
  * lệch kết luận khả thi/vô nghiệm             -> phải bị BẮT
  * lệch trong ngưỡng 1e-9 (1e-12)              -> được CHẤP NHẬN
  * cả hai solver cùng vô nghiệm                -> được CHẤP NHẬN
Các test này thay solver bằng hàm giả (monkeypatch), không cần binary.
"""
import pytest

import _harness
from _harness import cross_check, make_instance

# Ví dụ mục 6 của tài liệu: đáp án chuẩn 973.
INST = make_instance(n=5, m=3, w=[2, 3, 1, 4, 2], d=[3, 4, 2, 6, 1],
                     kappa=1.0, e_on=250.0, eps=1.0, cap=8.0, link_cap=5.0)


def fake_run(ref_energy=973.0, feasible=True, buggy_delta=None, buggy_feasible=None):
    """Dựng hàm run_solver giả: 'baseline' luôn đúng, 'dc' mang lỗi tuỳ chọn."""
    def _run(algo, inst):
        feas = feasible
        energy = ref_energy if feasible else None
        if algo == "dc":
            if buggy_feasible is not None:
                feas = buggy_feasible
                energy = ref_energy if feas else None
            elif buggy_delta is not None:
                energy = ref_energy + buggy_delta
        return {"feasible": feas, "energy": energy}
    return _run


def test_canary_bat_duoc_lech_energy_1e5(monkeypatch):
    """Solver sai lệch 1e-5 phải bị phát hiện."""
    monkeypatch.setattr(_harness, "run_solver", fake_run(buggy_delta=1e-5))
    with pytest.raises(AssertionError, match="energy"):
        cross_check(["baseline", "dc", "bruteforce"], [INST], tag="canary")


def test_canary_bat_duoc_lech_nguong_1e9(monkeypatch):
    """Lệch 1e-6 trên energy 973 vượt ngưỡng tương đối 1e-9 (9.73e-7) -> bị bắt."""
    monkeypatch.setattr(_harness, "run_solver", fake_run(buggy_delta=1e-6))
    with pytest.raises(AssertionError, match="energy"):
        cross_check(["baseline", "dc", "bruteforce"], [INST], tag="canary")


def test_canary_chap_nhan_lech_trong_nguong(monkeypatch):
    """Lệch 1e-12 (nhỏ hơn ngưỡng 1e-9) phải được chấp nhận - không báo giả lừa."""
    monkeypatch.setattr(_harness, "run_solver", fake_run(buggy_delta=1e-12))
    n_ok, n_inf = cross_check(["baseline", "dc", "bruteforce"], [INST], tag="canary")
    assert (n_ok, n_inf) == (1, 0)


def test_canary_bat_duoc_lech_feasible(monkeypatch):
    """Solver báo nhầm vô nghiệm <-> khả thi phải bị phát hiện."""
    monkeypatch.setattr(_harness, "run_solver", fake_run(buggy_feasible=False))
    with pytest.raises(AssertionError, match="feasible"):
        cross_check(["baseline", "dc", "bruteforce"], [INST], tag="canary")


def test_canary_vo_nghiem_dong_nhat():
    """Cả các solver cùng vô nghiệm: cross_check phải chấp nhận (0 nghiệm)."""
    monkeypatch = pytest.MonkeyPatch()
    monkeypatch.setattr(_harness, "run_solver", fake_run(feasible=False))
    try:
        n_ok, n_inf = cross_check(["baseline", "dc", "bruteforce"], [INST], tag="canary")
    finally:
        monkeypatch.undo()
    assert (n_ok, n_inf) == (0, 1)
