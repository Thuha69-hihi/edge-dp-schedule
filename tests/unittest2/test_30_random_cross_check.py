# -*- coding: utf-8 -*-
"""Cross-check ngẫu nhiên với seed cố định (tái lập được).

Sinh các instance ngẫu nhiên theo nhiều "chế độ":
  * loose      : tham số lỏng (gần như generator của tests/test_main.cpp)
  * window     : cap nằm giữa [w_max, S_N] -> cửa sổ trượt trên k (CHT/D&C)
  * tight_cap  : cap quanh w_max -> phần lớn VÔ NGHIỆM (deadline chặt)
  * tight_link : link_cap ~ 0..2 chặn hầu hết điểm cắt
Yêu cầu: mọi solver chính xác phải đồng nhất cả kết luận khả thi lẫn energy.
"""
import pytest

from _harness import (EXACT_CUBIC, EXACT_QUADRATIC, cross_check, ensure_built,
                      make_instance)

SEED_CUBIC = 20261030
SEED_QUAD = 20261031
SEED_LARGE = 20261032

@pytest.fixture(scope="module", autouse=True)
def _built():
    ensure_built()


def gen_cubic_small(rng, mode):
    n = rng.randint(1, 10)
    m = rng.randint(1, 5)
    w = [rng.randint(0, 8) for _ in range(n)]
    if rng.random() < 0.3:
        w[rng.randrange(n)] = rng.randint(1, 8)  # đảm bảo có tác vụ nặng
    d = [rng.randint(0, 8) for _ in range(n)]
    tot = sum(w)
    wmax = max(w)
    kappa = rng.choice([0.5, 1.0, 2.0])
    eps = rng.choice([0.5, 1.0, 2.0])
    e_on = float(rng.randint(0, 300))
    if mode == "loose":
        cap = float(rng.randint(8, 30))
        link = float(rng.randint(3, 10))
    elif mode == "window":
        cap = float(rng.randint(max(wmax, 1), max(tot, wmax, 1)))
        link = float(rng.randint(3, 10))
    elif mode == "tight_cap":
        cap = float(max(0, wmax - rng.randint(0, 2)))
        link = float(rng.randint(3, 10))
    elif mode == "tight_link":
        link = float(rng.choice([0, 1, 2]))
        cap = float(rng.choice([tot, max(0, tot - rng.randint(1, 10))]))
    else:
        raise ValueError(mode)
    return make_instance(n=n, m=m, w=w, d=d, kappa=kappa, e_on=e_on,
                         eps=eps, cap=cap, link_cap=link)


def test_random_cubic_3_solvers():
    """240 instance cubic nhỏ (N<=10): baseline vs dc vs bruteforce, seed cố định."""
    rng = pytest.importorskip("random").Random(SEED_CUBIC)
    modes = ["loose", "window", "tight_cap", "tight_link"]
    insts = [gen_cubic_small(rng, modes[t % 4]) for t in range(240)]
    n_ok, n_inf = cross_check(EXACT_CUBIC, insts, tag="rand_cubic")
    # Độ phủ: phải có cả nghiệm lẫn vô nghiệm (như baseline_vs_oracle_random của test_main.cpp)
    assert n_ok > 40, f"qua it truong hop co nghiem: {n_ok}"
    assert n_inf > 25, f"qua it truong hop vo nghiem: {n_inf}"
    print(f"\n[rand_cubic] co nghiem={n_ok}, vo nghiem={n_inf} / 240")


def test_random_quadratic_4_solvers():
    """180 instance quadratic nhỏ: baseline vs dc vs cht vs bruteforce (seed cố định).

    Một phần case giữ cap = 1e18 để đi nhánh deque của CHT, phần còn lại
    tạo cửa sổ trượt để đi nhánh cây bao lồi."""
    rng = pytest.importorskip("random").Random(SEED_QUAD)
    insts = []
    for t in range(180):
        mode = ["loose", "window", "tight_cap", "tight_link"][t % 4]
        inst = gen_cubic_small(rng, mode)
        if t % 3 == 0:
            inst["params"]["cap"] = 1e18  # không bao giờ cắt cửa sổ -> nhánh deque
        inst["params"]["model"] = "quadratic"
        inst["params"]["quad_a"] = rng.choice([0.0, 0.5, 1.0, 2.0])
        inst["params"]["quad_b"] = rng.choice([0.0, 1.0, 3.0])
        insts.append(inst)
    n_ok, n_inf = cross_check(EXACT_QUADRATIC, insts, tag="rand_quad")
    assert n_ok > 40 and n_inf > 25
    print(f"\n[rand_quad] co nghiem={n_ok}, vo nghiem={n_inf} / 180")


def gen_large(rng, model):
    n = rng.randint(60, 150)
    m = rng.randint(1, 8)
    w = [rng.randint(0, 20) for _ in range(n)]
    d = [rng.randint(0, 10) for _ in range(n)]
    tot = sum(w)
    inst = make_instance(
        n=n, m=m, w=w, d=d, kappa=rng.choice([0.5, 1.0, 2.0]),
        e_on=float(rng.randint(0, 5000)), eps=rng.choice([0.5, 1.0, 2.0]),
        cap=float(rng.randint(max(max(w), 1), max(tot, 1))),
        link_cap=float(rng.randint(3, 10)))
    if model == "quadratic":
        inst["params"]["model"] = "quadratic"
        inst["params"]["quad_a"] = rng.choice([0.0, 0.5, 1.0, 2.0])
        inst["params"]["quad_b"] = rng.choice([0.0, 1.0, 3.0])
        mode = rng.randrange(3)  # giống cht_larger_inputs của test_main.cpp
        per = tot // max(1, m)
        inst["params"]["cap"] = float([per + 25, 2 * per + 25, tot + 1][mode])
    return inst


def test_random_large_cubic_baseline_vs_dc():
    """30 instance cubic N<=150: bruteforce bị loại (N>18), so baseline vs dc."""
    import random
    rng = random.Random(SEED_LARGE)
    insts = [gen_large(rng, "cubic") for _ in range(30)]
    n_ok, n_inf = cross_check(["baseline", "dc"], insts, tag="rand_large_cubic")
    assert n_ok + n_inf == 30
    print(f"\n[rand_large_cubic] co nghiem={n_ok}, vo nghiem={n_inf} / 30")


def test_random_large_quadratic_baseline_dc_cht():
    """25 instance quadratic N<=150: baseline vs dc vs cht (cả hai nhánh cap)."""
    import random
    rng = random.Random(SEED_LARGE + 7)
    insts = [gen_large(rng, "quadratic") for _ in range(25)]
    n_ok, n_inf = cross_check(["baseline", "dc", "cht"], insts, tag="rand_large_quad")
    assert n_ok + n_inf == 25
    print(f"\n[rand_large_quad] co nghiem={n_ok}, vo nghiem={n_inf} / 25")
