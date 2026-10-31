# Báo cáo độ phủ kiểm thử (việc 7.02)

Đo trên `main` sau PR #20, gcc 13.3 `-O0 --coverage`, gcovr 8. Chạy lại: `make coverage`
(cần `pip install gcovr`; xong thì `make clean && make` để quay lại bản `-O2`).

Bộ kiểm thử gồm `make test` (31 test C++, 1 SKIP có chủ đích: CHT không áp dụng cho mô hình bậc ba)
và `pytest tests/unittest2` (41 test gọi CLI). Đo trên toàn bộ mã thư viện và CLI `edgedp`;
không tính `main_heuristics.cpp` (công cụ in bảng so sánh, không phải phần được kiểm thử).

**Tổng: 99% dòng (732/739), 95% nhánh (735/767).** Số nhánh đã bỏ các nhánh ngoại lệ do trình
biên dịch sinh ra (`--exclude-throw-branches --exclude-unreachable-branches`).

| Tệp | Dòng | Phủ dòng | Nhánh | Phủ nhánh |
|---|---|---|---|---|
| include/edgedp/cost.hpp | 16/16 | 100% | 13/13 | 100% |
| src/baseline_dp.cpp | 61/61 | 100% | 48/48 | 100% |
| src/brute_force.cpp | 48/48 | 100% | 38/38 | 100% |
| src/cht_dp.cpp | 167/167 | 100% | 139/141 | 98% |
| src/dc_opt.cpp | 101/106 | 95% | 84/86 | 97% |
| src/heuristics.cpp | 101/103 | 98% | 110/114 | 96% |
| src/io.cpp | 91/91 | 100% | 153/170 | 90% |
| src/json.cpp | 107/107 | 100% | 99/100 | 99% |
| src/main.cpp | 36/36 | 100% | 47/50 | 94% |
| src/registry.cpp | 3/3 | 100% | 4/7 | 57% |

Trước việc này: 79% dòng, `heuristics.cpp` 0%, `main.cpp` 66%, `json.cpp` 77%.

## Test bổ sung

`tests/test_main.cpp` (9 test):

- `all_solvers_empty_and_no_nodes`: N = 0 và M = 0 cho mọi solver (trước chỉ kiểm baseline).
- `example_quadratic_all_solvers`: ví dụ chuẩn bậc hai, mọi solver cho 579, cắt tại 2.
- `bruteforce_rejects_large_n`, `cht_rejects_negative_a`: hai lỗi đầu vào của solver.
- `heuristics_example_values`, `heuristics_trivial_cases`: 4 heuristic trên ví dụ chuẩn
  (973 / 1051 / 1981 / 1108, số vi phạm) và trường hợp N = 0, M = 0; tên heuristic sai bị từ chối.
- `heuristics_never_beat_optimum`: 300 bộ ngẫu nhiên; heuristic liên tiếp nào khả thi cũng
  không rẻ hơn nghiệm tối ưu, và năng lượng nó báo bằng tổng chi phí các đoạn theo điểm cắt.
- `json_parser_edge_cases`, `instance_json_rejects_invalid_values`: literal, escape, mảng/đối tượng
  rỗng, số mũ; 8 chuỗi JSON hỏng; w âm/không nguyên/không phải số, m âm, thiếu tham số, sai độ dài d.

`tests/unittest2/test_50_cli.py` (12 test): mã thoát của CLI đúng như README
(0 kể cả vô nghiệm với `energy` = null, 2 cho sai tham số/file không tồn tại/JSON hỏng,
3 cho CHT với bậc ba và vét cạn với N > 18), solver mặc định là baseline.

## Phần chưa phủ

- `dc_opt.cpp` dòng 59–63: nhánh dự phòng khi một hàng không có ứng viên hữu hạn. Theo Mục 4.4 của
  báo cáo, sau khi lọc tập ứng viên P mọi hàng trong R đều có ít nhất một ứng viên hữu hạn, nên nhánh
  này không xảy ra khi giả thiết đúng; được giữ để chương trình không sai nếu giả thiết bị vi phạm.
- Các nhánh còn lại chủ yếu là kiểm tra lỗi hiếm trong `io.cpp` (kiểu sai của từng trường tham số)
  và `registry.cpp` (vòng tìm solver theo tên, đã phủ qua CLI nhưng gcov tính nhánh theo từng phần tử).

## Ghi chú phát hiện khi viết test

Round-robin giao tác vụ không liên tiếp, nằm ngoài mô hình pipeline, nên khi nó "khả thi" có thể rẻ hơn
nghiệm tối ưu của bài toán chia chuỗi (gặp 1 trong 300 bộ ngẫu nhiên). Trong lưới thí nghiệm của báo cáo,
round-robin khả thi 0/300 bộ nên không ảnh hưởng số liệu Mục 6.4, nhưng không nên diễn giải Energy Saved %
của round-robin như so sánh cùng mô hình.
