# Khảo sát nguồn dữ liệu mô phỏng

Nội dung so sánh các nguồn dữ liệu mà nhóm cân nhắc dùng cho thí nghiệm
(ví dụ Google Cluster Data 2011, Google Borg trace 2019, Alibaba cluster-trace-v2018 và
dữ liệu IoT tổng hợp) và chốt tham số mô phỏng để `scripts/gen_data.py` và
`solver*.cpp` nhận đầu vào nhất quán.

Mục đích so sánh: chọn phân phối `w_i` (chu kỳ CPU), `d_i` (bit dữ liệu), `deadline`,
và `M` (số edge node) phù hợp với đặc tả đề tài "Energy-Aware Task Scheduling in Edge
Computing", tránh lấy số từ cluster lớn (95% percentile) sẽ phá mô hình bậc ba vì
`W³` phình quá nhanh.

## 1. Google Cluster Data 2011

- Phát hành tháng 5/2011, gồm log 29 ngày của 11k+ máy tại Google (Reiss et al., 2012).
- Đơn vị tài nguyên ảo (CPU = 0..1 normalized). Cấu trúc: scheduler events + task
  usage + machine events. Phù hợp với nghiên cứu scheduler nhưng phân bố rất rộng
  (CPU usage heavy-tail; phần lớn tác vụ nhỏ, một số đầu tác vụ rất lớn) [1].
- Hạn chế khi áp cho đề tài:
  * Mỗi job là DAG, không phải chuỗi tuần tự `τ_1 → … → τ_N` của ta.
  * Không có deadline cố định theo chu kỳ T mà có priority + scheduling class.
- Khi ánh xạ sang mô hình bài toán, cần: (a) flatten DAG thành critical path,
  (b) ánh xạ CPU cycles → `w_i` chu kỳ chuẩn hoá, (c) thiết lập `deadline = T` cho cả
  đường. Mức ánh xạ này thường được thực hiện trong pipeline riêng.

## 2. Google Borg Trace 2019

- Tám cluster công khai, ~2.4 TiB dữ liệu; định dạng Parquet + protobuf (TLA+
  schemas) [2]. Đa dạng workload hơn 2011: cả long-running service lẫn batch.
- Có metadata đầy đủ hơn (resource requests vs actual), hỗ trợ phân tích
  over-subscription. Cũng có priority class mapping.
- Vẫn là DAG, vẫn heavy-tail: giá trị CPU trung vị thấp, đuôi rất dài.
- Hạn chế tương tự: cần flatten DAG, mất đi tính tuần tự.

## 3. Alibaba cluster-trace-v2018

- Công khai tháng 7/2018, ~4000 máy, 8 ngày; cả online service và batch jobs;
  công kể DAG của batch jobs [3].
- Có hai phiên bản 2017 (24 giờ, co-located) và 2018 (8 ngày). DAG batch jobs
  đầy đủ giúp ánh xạ trực tiếp sang bài toán phân đoạn hơn trace Google.
- Tuy nhiên `w_i` thường normalized theo machine capacity, cần map về chu kỳ
  CPU thực của edge node (loại Cortex-M, RPi, ...).

## 4. Dữ liệu IoT tổng hợp (synthetic cho edge computing)

- Dùng khi không cần tái hiện workload thực, hoặc khi muốn kiểm soát exactly
  phân phối `w_i`/`d_i` từng biến.
- Một số nghiên cứu đề xuất phân phối heavy-tail (log-normal / Pareto) cho
  computation demand và deadline ngặt cho time-critical IoT task [4].
- Trong nhóm, `scripts/gen_data.py` sinh `w_i, d_i` U(1, max_w) và U(0, max_d)
  cho thử nhanh. Đề xuất tinh chỉnh:
  * `w_i`: heavy-tail, e.g. `min(W_max, round(Gamma(shape=1, scale=2)))` hoặc
    `log-normal`; một số task rất nhỏ (sensor ping) và một số task nặng
    (machine learning inference).
  * `d_i`: phi heavy-tail nhẹ hơn; thường `d_i ~ K * w_i` (dữ liệu tỉ lệ khối
    lượng tính toán).
  * `deadline = T`: cố định theo mô hình, không random.
  * `M`: 4, 8, 16, 32 (lưới thí nghiệm 5.01).
  * `E_on`, `ε`, `κ` đã chốt trong tests; tỉ lệ giữa ba đại lượng chính là
    yếu tố quyết định "có khi nào tối ưu dùng hết M node?".

## 5. Chốt tham số mô phỏng (tham khảo, có thể cập nhật khi TV5 chạy 2.06)

| Symbol | Ý nghĩa | Đề xuất | Ghi chú |
|---|---|---|---|
| N | Số tác vụ | 10², 10³, 10⁴, 10⁵ | lưới nhỏ brute force kiểm; lưới lớn benchmark |
| M | Số edge node | 4, 8, 16, 32 | đủ nhỏ để có hành vi "không dùng hết M" |
| w_i | Chu kỳ CPU | Heavy-tail (Gamma) | đuôi cắt tại `cap = f_max·T` |
| d_i | Bit dữ liệu | `min(d_max, ⌈α·w_i⌉)` | tỉ lệ với w_i |
| T | Deadline/đoạn | cố định | 1 giây đơn vị chuẩn hoá |
| E_on | Bật node | 0..500 | dải rộng để có cả "dùng hết M" lẫn "không dùng hết M" |
| ε | Truyền/bit | 0.5..2.0 | giữ trong tỉ lệ nhỏ so với E_exec |
| κ | Công suất động | 0.5..2.0 | chia cho T² thành `kappa_over_T2` |
| cap | `f_max·T` | ≥ max(w_i), thường ~1.5·(Σw/M) | đảm bảo luôn có nghiệm |
| link_cap | `B·T` | ≥ max(d_i) mặc định; giảm xuống khi muốn "cắt → d_k > link_cap" thành ràng buộc chặt |

## Tài liệu tham khảo

1. Reiss, C., Tumanov, A., Ganger, G. R., Katz, R. H., & Kozuch, M. A. (2012).
   *Towards Understanding Heterogeneous Clouds at Scale: a Google Production
   Trace Analysis*. CMU-PDL Technical Report ISTC-CC-TR-12-101.
   http://www.pdl.cmu.edu/ftp/CloudComputing/ISTC-CC-TR-12-101.pdf
2. Tirmazi, M. et al. (2020). *Borg: the Next Generation*. Eurosys 2020.
   Trace dataset: https://github.com/google/cluster-data
3. Alibaba Cluster Data v2018: https://github.com/alibaba/clusterdata
4. Mutichiro, B. & Bappy, F. H. et al. (2021/2023). *IoT-Edge scheduling workload
   synthetic generation guidelines*. (tham khảo thêm khi TV5 lock thiết kế)
