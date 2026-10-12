// Nhiệm vụ 2.08 - Các thuật toán cơ sở theo kinh nghiệm (heuristic baselines) để tính % Năng lượng Tiết kiệm (Thành viên 5).
// Mô hình: Chuỗi N tác vụ được cắt thành tối đa M phân đoạn liên tiếp (<= M), mỗi phân đoạn trên một nút Biên (Edge node).
//   Cost(k,i] = A*W^2 + ETX*d_k   (W = S_i - S_k; xấp xỉ DVFS bậc hai, cộng chi phí truyền ETX*d_k tại ranh giới k>0)
//   Phân đoạn (k,i] khả thi khi và chỉ khi W/F_MAX <= dl[i]-dl[k]   (với dl[0]=0)
// Các thuật toán Heuristic: greedy (tham lam), even (chia đều), single (gom vào 1 node), rr (round-robin / xoay vòng).
// Tùy chọn "dp" = thuật toán DP tham chiếu độ phức tạp O(M*N^2)
// (dùng tạm trong lúc chờ bộ giải baseline 2.03 được gộp code; chỉ nên dùng với N <= ~5000).
// Biên dịch: g++ -O2 -std=c++17 -o heuristics src/heuristics.cpp
// Chạy:      ./heuristics data/x.csv M [--dp]    -> Xuất các dòng CSV: tên_thuật_toán,năng_lượng,số_lần_vi_phạm,phần_trăm_tiết_kiệm_so_với_<tham_chiếu>
#include <bits/stdc++.h>
using namespace std;
typedef long double ld;
static const ld A = 1e-17L, ETX = 5e-8L, F_MAX = 1e9L, INF = 1e30L;

struct Inst { int n; vector<ld> w, d, dl, S; };

Inst load(const string& path) {
    ifstream f(path); if (!f) { cerr << "cannot open " << path << "\n"; exit(1); }
    Inst I; string line; getline(f, line);
    I.w = {0}; I.d = {0}; I.dl = {0};
    while (getline(f, line)) {
        if (line.empty()) continue;
        stringstream ss(line); string t; vector<ld> v;
        while (getline(ss, t, ',')) v.push_back(stold(t));
        I.w.push_back(v[1]); I.d.push_back(v[2]); I.dl.push_back(v[3]);
    }
    I.n = (int)I.w.size() - 1; I.S.assign(I.n + 1, 0);
    for (int i = 1; i <= I.n; i++) I.S[i] = I.S[i-1] + I.w[i];
    return I;
}
ld cost(const Inst& I, int k, int i) {                 // segment (k, i], 0<=k<i<=n
    ld W = I.S[i] - I.S[k];
    if (W / F_MAX > I.dl[i] - I.dl[k] + 1e-12L) return INF;   // deadline violated
    return A * W * W + (k > 0 ? ETX * I.d[k] : 0);
}
// energy ignoring deadline (heuristics are evaluated, not constrained); violations counted separately
ld rawcost(const Inst& I, int k, int i) { ld W = I.S[i]-I.S[k]; return A*W*W + (k>0?ETX*I.d[k]:0); }
bool viol(const Inst& I, int k, int i) { return cost(I,k,i) >= INF; }

struct Res { ld e; int v; int segs; };
Res eval(const Inst& I, const vector<int>& cuts) {      // cuts: 0=c0<c1<...<cs=n
    Res r{0, 0, (int)cuts.size() - 1};
    for (size_t j = 1; j < cuts.size(); j++) { r.e += rawcost(I, cuts[j-1], cuts[j]); r.v += viol(I, cuts[j-1], cuts[j]); }
    return r;
}
Res single(const Inst& I) { return eval(I, {0, I.n}); }
Res even(const Inst& I, int M) {
    M = min(M, I.n); vector<int> c{0};
    for (int j = 1; j <= M; j++) c.push_back((int)((long long)I.n * j / M));
    return eval(I, c);
}
// Greedy: close the current segment as soon as its workload reaches (remaining workload / nodes left).
Res greedy(const Inst& I, int M) {
    vector<int> c{0}; int k = 0, left = M;
    for (int i = 1; i < I.n && left > 1; i++) {
        ld rem = I.S[I.n] - I.S[k];
        if (I.S[i] - I.S[k] >= rem / left) { c.push_back(i); k = i; left--; }
    }
    c.push_back(I.n); return eval(I, c);
}
// Round-robin: task i -> node (i-1)%M. Non-contiguous: node energy A*(load)^2, transfer for every hop.
Res rr(const Inst& I, int M) {
    M = min(M, I.n); vector<ld> L(M, 0); Res r{0, 0, M};
    for (int i = 1; i <= I.n; i++) { L[(i-1)%M] += I.w[i]; if (i < I.n && M > 1) r.e += ETX * I.d[i]; }
    for (int m = 0; m < M; m++) { r.e += A * L[m] * L[m]; if (L[m] / F_MAX > I.dl[I.n] + 1e-12L) r.v++; }
    return r;
}
Res dp(const Inst& I, int M) {                              // reference optimum with deadlines
    int n = I.n; vector<vector<ld>> D(M + 1, vector<ld>(n + 1, INF)); D[0][0] = 0;
    vector<vector<int>> P(M + 1, vector<int>(n + 1, 0));
    for (int j = 1; j <= M; j++) for (int i = 1; i <= n; i++) for (int k = j - 1; k < i; k++) {
        if (D[j-1][k] >= INF) continue;
        ld c = cost(I, k, i);
        if (c >= INF) continue;
        if (D[j-1][k] + c < D[j][i]) { D[j][i] = D[j-1][k] + c; P[j][i] = k; }
    }
    int bj = -1; ld best = INF;
    for (int j = 1; j <= M; j++) if (D[j][n] < best) { best = D[j][n]; bj = j; }
    if (bj < 0) return Res{INF, -1, 0};
    return Res{best, 0, bj};
}
int main(int argc, char** argv) {
    if (argc < 3) { cerr << "usage: heuristics file.csv M [--dp]\n"; return 1; }
    Inst I = load(argv[1]); int M = atoi(argv[2]); bool useDp = argc > 3 && string(argv[3]) == "--dp";
    if (argc > 4 && string(argv[3]) == "--only") {      // harness mode: ./heuristics f.csv M --only dp|greedy|even|single|rr
        string nm = argv[4]; Res r = nm=="dp"?dp(I,M): nm=="greedy"?greedy(I,M): nm=="even"?even(I,M): nm=="single"?single(I): rr(I,M);
        cout << setprecision(15) << "RESULT energy=" << (double)r.e << " violations=" << r.v << "\n"; return 0;
    }
    vector<pair<string, Res>> out = {{"greedy", greedy(I, M)}, {"even", even(I, M)},
                                     {"single", single(I)}, {"rr", rr(I, M)}};
    cout << setprecision(12) << "name,energy,violations,segments,saved_pct_vs_ref\n";
    Res ref{0,0,0}; string refname;
    if (useDp) { ref = dp(I, M); refname = "dp"; out.insert(out.begin(), {"dp", ref}); }
    for (auto& [nm, r] : out) {
        double saved = (useDp && nm != "dp" && r.e > 0 && ref.e < INF) ? (double)((r.e - ref.e) / r.e * 100) : NAN;
        cout << nm << "," << (double)r.e << "," << r.v << "," << r.segs << "," << saved << "\n";
    }
}
