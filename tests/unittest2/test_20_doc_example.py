# -*- coding: utf-8 -*-
"""Hồi quy với ví dụ chuẩn trong tài liệu mô hình (mục 6): tests/data/example_n5.json.

Đáp án chuẩn: energy = 973, 2 node, cắt sau tau_2 (đoạn 378 + 595).
Mọi solver chính xác phải cho cùng kết quả này.
"""
import math
import os

import pytest

from _harness import ROOT, EXACT_CUBIC, cross_check, ensure_built, run_solver

EXAMPLE = os.path.join(ROOT, "tests", "data", "example_n5.json")

@pytest.fixture(scope="module", autouse=True)
def _built():
    ensure_built()


def test_doc_example_cross_solver_cubic():
    """Ví dụ mục 6 của tài liệu: 973, 2 node, cuts=[2] trên MÔ HÌNH BẬC BA."""
    import json
    with open(EXAMPLE) as f:
        inst = json.load(f)
    n_ok, n_inf = cross_check(EXACT_CUBIC, [inst], tag="doc_n5")
    assert (n_ok, n_inf) == (1, 0)
    for algo in EXACT_CUBIC:
        sol = run_solver(algo, inst)
        assert math.isclose(sol["energy"], 973, rel_tol=1e-9, abs_tol=1e-9), algo
        assert sol["nodes_used"] == 2, algo
        assert sol["cuts"] == [2], algo


def test_doc_example_json_giong_file():
    """CLI chạy trực tiếp trên file example_n5.json cũng ra 973 (đúng README)."""
    import subprocess
    out = subprocess.run([run_solver.__globals__["EDGEDP"], "--algo", "baseline",
                          "--input", EXAMPLE], capture_output=True, text=True, check=True)
    import json
    sol = json.loads(out.stdout)
    assert math.isclose(sol["energy"], 973, rel_tol=1e-9, abs_tol=1e-9)
    assert sol["cuts"] == [2]


def test_doc_example_bien_the_eon_thap():
    """Biến thể của ví dụ trong test_main.cpp: E_on = 200 thì tối ưu dùng 3 node
    (2 node = 873, 3 node = 864)."""
    inst = {
        "n": 5, "m": 3,
        "w": [2, 3, 1, 4, 2], "d": [3, 4, 2, 6, 1],
        "params": {"kappa_over_T2": 1.0, "e_on": 200.0, "eps": 1.0,
                   "cap": 8.0, "link_cap": 5.0},
    }
    cross_check(EXACT_CUBIC, [inst], tag="doc_eon200")
    for algo in EXACT_CUBIC:
        sol = run_solver(algo, inst)
        assert math.isclose(sol["energy"], 864, rel_tol=1e-9, abs_tol=1e-9), algo
        assert sol["nodes_used"] == 3 and sol["cuts"] == [2, 4], algo
