# -*- coding: utf-8 -*-
"""Kiểm thử giao diện dòng lệnh build/edgedp (việc 7.02).

Mã thoát theo README: 0 = thành công (kể cả vô nghiệm), 2 = sai tham số hoặc lỗi đọc
đầu vào, 3 = solver không áp dụng được cho instance (cht với mô hình bậc ba,
bruteforce với N > 18).
"""
import json
import os
import subprocess

import pytest

from _harness import EDGEDP, ROOT, ensure_built

EXAMPLE = os.path.join(ROOT, "tests", "data", "example_n5.json")
EXAMPLE_Q = os.path.join(ROOT, "tests", "data", "example_n5_quadratic.json")


@pytest.fixture(scope="module", autouse=True)
def _built():
    ensure_built()


def cli(*args):
    return subprocess.run([EDGEDP, *args], capture_output=True, text=True, timeout=60)


def test_mac_dinh_la_baseline():
    p = cli("--input", EXAMPLE)
    assert p.returncode == 0
    out = json.loads(p.stdout)
    assert out["solver"] == "baseline" and out["energy"] == 973


@pytest.mark.parametrize("args", [
    [],                                   # thiếu --input
    ["--algo", "dc"],                     # thiếu --input
    ["--input"],                          # cờ thiếu giá trị
    ["--khong-co", "x"],                  # cờ lạ
    ["--algo", "khong_co", "--input", EXAMPLE],
])
def test_sai_tham_so_tra_ve_2(args):
    p = cli(*args)
    assert p.returncode == 2
    assert p.stdout == ""


def test_file_khong_ton_tai_tra_ve_2(tmp_path):
    p = cli("--input", str(tmp_path / "khong_co.json"))
    assert p.returncode == 2 and "Loi input" in p.stderr


def test_json_hong_tra_ve_2(tmp_path):
    f = tmp_path / "hong.json"
    f.write_text('{"n": 2, "m": 1, "w": [1, 2]')
    p = cli("--input", str(f))
    assert p.returncode == 2 and "Loi input" in p.stderr


def test_cht_voi_bac_ba_tra_ve_3():
    p = cli("--algo", "cht", "--input", EXAMPLE)
    assert p.returncode == 3 and "quadratic" in p.stderr


def test_cht_voi_bac_hai_tra_ve_579():
    p = cli("--algo", "cht", "--input", EXAMPLE_Q)
    assert p.returncode == 0 and json.loads(p.stdout)["energy"] == 579


def test_bruteforce_n_lon_tra_ve_3(tmp_path):
    n = 19
    inst = {"n": n, "m": 2, "w": [1] * n, "d": [0] * n,
            "params": {"kappa_over_T2": 1, "e_on": 1, "eps": 1, "cap": 100, "link_cap": 100}}
    f = tmp_path / "n19.json"
    f.write_text(json.dumps(inst))
    p = cli("--algo", "bruteforce", "--input", str(f))
    assert p.returncode == 3
    assert cli("--algo", "dc", "--input", str(f)).returncode == 0


def test_vo_nghiem_van_tra_ve_0_va_energy_null(tmp_path):
    inst = json.load(open(EXAMPLE))
    inst["m"] = 1  # tổng khối lượng 12 > cap 8, một node không đủ
    f = tmp_path / "m1.json"
    f.write_text(json.dumps(inst))
    p = cli("--algo", "dc", "--input", str(f))
    assert p.returncode == 0
    out = json.loads(p.stdout)
    assert out["feasible"] is False and out["energy"] is None
