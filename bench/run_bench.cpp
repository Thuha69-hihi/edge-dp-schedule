// bench/run_bench.cpp - Benchmark harness (việc 3.06, TV5), bản C++ của run_bench.py.
//
// Chạy các solver (qua CLI build/edgedp) trên nhiều instance và xuất CSV:
//   * time_ms: do chính solver đo bằng std::chrono (chỉ phần giải) + wall_ms do harness đo (cả tiến trình);
//   * bộ nhớ đỉnh (RSS): ưu tiên `/usr/bin/time -v` (mem_src = time), nếu không có thì lấy mẫu
//     VmHWM của tiến trình con trong /proc (mem_src = proc, Linux; gần đúng với lần chạy < ~10 ms).
//     Không dùng ru_maxrss của wait4: sau fork() tiến trình con thừa hưởng RSS của cha;
//   * đối chiếu: solver "chính xác" (baseline, dc, cht) phải cho cùng năng lượng (sai số tương
//     đối <= 1e-9) -> cột match_ref; heuristic có thêm energy_saved_pct (chỉ khi heuristic khả thi).
//
// Hợp đồng với CLI (src/main.cpp):  <bin> --algo NAME --input FILE.json
// in ra stdout JSON có: solver, feasible, energy (null nếu vô nghiệm), nodes_used, time_ms
// (heuristic: thêm violated_cap, violated_link). Mã thoát 3 = solver chưa cài (status not_impl).
// Heuristic (greedy, equal, single, roundrobin) chưa có trong CLI nên mặc định không chạy.
//
// Build (từ thư mục gốc dự án, json.cpp/json.hpp nằm cạnh io.cpp):
//   g++ -std=c++17 -O2 -Isrc bench/run_bench.cpp src/json.cpp -o build/run_bench
// Ví dụ:
//   build/run_bench --bin build/edgedp data/synthetic --repeats 3 --max-n baseline=10000
//       --timeout 600 --out results/time_mem.csv --summary results/time_mem_summary.csv
//   build/run_bench --bin build/edgedp data/small/manifest.csv --solvers baseline,dc
// Đầu vào: file .json, thư mục (mọi *.json, đệ quy), glob, hoặc manifest.csv do gen_data.py sinh.
// Đo bộ nhớ cần Linux (Ubuntu: `sudo apt install time` nếu thiếu /usr/bin/time).
#include <fcntl.h>
#include <glob.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "json.hpp"

namespace fs = std::filesystem;
using edgedp::Json;
using Opt = std::optional<double>;

static const char* DEFAULT_SOLVERS = "baseline,dc,cht";
static const char* DEFAULT_EXACT = "baseline,dc,cht";
static const std::set<std::string> HEURISTICS = {"greedy", "equal", "single", "roundrobin"};
static const double REL_TOL = 1e-9;
static const char* TIME_BIN = "/usr/bin/time";
static const char* CLI_FLAG = "--algo";

struct Row {
    std::string file, source, solver, status, note, mem_src, match_ref;
    Opt alpha, beta;
    Opt n, m, seed;
    int rep = 1;
    std::optional<bool> feasible;
    Opt energy, nodes_used, time_ms, wall_ms, peak_rss, load_rss, net_rss, vcap, vlink, saved;
};

// ----------------------------------------------------------------------------- định dạng
static std::string fnum(const Opt& v, const char* fmt) {
    if (!v) return "";
    char buf[64];
    std::snprintf(buf, sizeof buf, fmt, *v);
    return buf;
}

static std::string csv_escape(const std::string& s) {
    if (s.find_first_of(",\"\n\r") == std::string::npos) return s;
    std::string o = "\"";
    for (char c : s) {
        if (c == '"') o += '"';
        o += c;
    }
    return o + "\"";
}

static std::string trim(std::string s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

static std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    std::stringstream ss(s);
    while (std::getline(ss, cur, sep)) out.push_back(cur);
    if (!s.empty() && s.back() == sep) out.push_back("");
    return out;
}

// ----------------------------------------------------------------------------- chạy 1 lần
static std::string mem_mode = "none";

struct RunResult {
    std::string status = "ok";  // ok | timeout
    std::string out, err;
    double wall_ms = 0;
    Opt rss_mb;
    int rc = -1;
};

static std::string read_file_str(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

static std::string read_tmp(FILE* f) {
    std::fflush(f);
    std::rewind(f);
    std::string s;
    char buf[65536];
    size_t k;
    while ((k = std::fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, k);
    return s;
}

static Opt vmhwm_mb(pid_t pid, const std::string& self_cmdline) {
    const std::string base = "/proc/" + std::to_string(pid);
    std::string cl = read_file_str(base + "/cmdline");
    if (cl.empty() || cl == self_cmdline) return std::nullopt;  // còn là bản sao giữa fork() và exec()
    std::ifstream f(base + "/status");
    std::string line;
    while (std::getline(f, line))
        if (line.rfind("VmHWM:", 0) == 0) return std::atof(line.c_str() + 6) / 1024.0;
    return std::nullopt;
}

static RunResult run_once(std::vector<std::string> cmd, double timeout) {
    if (mem_mode == "time") cmd.insert(cmd.begin(), {TIME_BIN, "-v"});
    const std::string self_cmdline = mem_mode == "proc" ? read_file_str("/proc/self/cmdline") : "";
    FILE* fo = std::tmpfile();
    FILE* fe = std::tmpfile();
    RunResult r;
    if (!fo || !fe) {
        r.status = "error";
        return r;
    }
    std::vector<char*> argv;
    for (auto& s : cmd) argv.push_back(const_cast<char*>(s.c_str()));
    argv.push_back(nullptr);

    auto t0 = std::chrono::steady_clock::now();
    auto elapsed = [&] { return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(); };
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        int dn = open("/dev/null", O_RDONLY);
        if (dn >= 0) dup2(dn, 0);
        dup2(fileno(fo), 1);
        dup2(fileno(fe), 2);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    int st = 0;
    double delay = 0.0005;
    for (;;) {
        if (mem_mode == "proc")
            if (Opt v = vmhwm_mb(pid, self_cmdline)) r.rss_mb = v;  // VmHWM chỉ tăng: lấy lần đọc cuối
        if (waitpid(pid, &st, WNOHANG) == pid) break;
        if (timeout > 0 && elapsed() > timeout) {
            r.status = "timeout";
            kill(-pid, SIGKILL);
            kill(pid, SIGKILL);
            waitpid(pid, &st, 0);
            break;
        }
        timespec ts{0, static_cast<long>(delay * 1e9)};
        nanosleep(&ts, nullptr);
        delay = std::min(delay * 2, mem_mode == "proc" ? 0.002 : 0.02);
    }
    r.wall_ms = elapsed() * 1000.0;
    r.out = read_tmp(fo);
    r.err = read_tmp(fe);
    std::fclose(fo);
    std::fclose(fe);
    r.rc = r.status == "ok" && WIFEXITED(st) ? WEXITSTATUS(st) : -1;
    if (mem_mode == "time" && r.status == "ok") {
        const std::string key = "Maximum resident set size";
        size_t p = r.err.find(key);
        if (p != std::string::npos) {
            size_t c = r.err.find(':', p);
            if (c != std::string::npos) r.rss_mb = std::atof(r.err.c_str() + c + 1) / 1024.0;
        }
    }
    return r;
}

static void detect_mem_mode(const std::string& requested) {
    if (requested != "auto") {
        mem_mode = requested;
        return;
    }
    mem_mode = "none";
    if (fs::exists(TIME_BIN)) {
        RunResult r = run_once({TIME_BIN, "-v", "true"}, 30);
        if (r.err.find("Maximum resident set size") != std::string::npos) {
            mem_mode = "time";
            return;
        }
    }
    if (fs::is_directory("/proc/self")) mem_mode = "proc";
}

// ----------------------------------------------------------------------------- đầu vào
static std::vector<std::string> collect_inputs(const std::vector<std::string>& paths) {
    std::vector<std::string> files;
    for (const auto& p : paths) {
        if (fs::is_directory(p)) {
            std::vector<std::string> sub;
            for (auto& e : fs::recursive_directory_iterator(p))
                if (e.is_regular_file() && e.path().extension() == ".json") sub.push_back(e.path().string());
            std::sort(sub.begin(), sub.end());
            files.insert(files.end(), sub.begin(), sub.end());
        } else if (p.size() > 4 && p.substr(p.size() - 4) == ".csv") {
            std::ifstream f(p);
            std::string line;
            std::getline(f, line);
            auto head = split(trim(line), ',');
            size_t col = std::find(head.begin(), head.end(), "file") - head.begin();
            if (col == head.size()) {
                std::cerr << "manifest thiếu cột file: " << p << "\n";
                std::exit(2);
            }
            std::string base = fs::path(p).parent_path().string();
            while (std::getline(f, line)) {
                auto cells = split(trim(line), ',');
                if (col < cells.size() && !cells[col].empty())
                    files.push_back(base.empty() ? cells[col] : (fs::path(base) / cells[col]).string());
            }
        } else if (p.find_first_of("*?[") != std::string::npos) {
            glob_t g;
            if (glob(p.c_str(), 0, nullptr, &g) == 0)
                for (size_t i = 0; i < g.gl_pathc; ++i) files.push_back(g.gl_pathv[i]);
            globfree(&g);
        } else {
            files.push_back(p);
        }
    }
    std::set<std::string> seen;
    std::vector<std::string> out;
    for (auto& f : files)
        if (seen.insert(f).second) out.push_back(f);
    return out;
}

static Opt json_num(const Json* j) {
    if (j && j->type == Json::Num) return static_cast<double>(j->num);
    return std::nullopt;
}

static Row read_header(const std::string& path) {
    Row r;
    r.file = fs::path(path).filename().string();
    Json root = edgedp::parse_json(read_file_str(path));
    r.n = json_num(root.get("n"));
    r.m = json_num(root.get("m"));
    r.seed = json_num(root.get("seed"));
    if (const Json* s = root.get("source"))
        if (s->type == Json::Str) r.source = s->str;
    if (const Json* meta = root.get("meta")) {
        r.alpha = json_num(meta->get("alpha"));
        r.beta = json_num(meta->get("beta"));
    }
    return r;
}

// ----------------------------------------------------------------------------- chạy 1 file
struct Args {
    std::vector<std::string> inputs;
    std::string bin, solvers = DEFAULT_SOLVERS, exact = DEFAULT_EXACT, load_solver;
    std::string mem_mode = "auto", out = "results/time_mem.csv", summary;
    int repeats = 1;
    double timeout = 600.0;
    std::map<std::string, long long> max_n;
};

static std::string alias(const std::string& s) { return s == "dnc" ? "dc" : s; }  // tên cũ -> registry.cpp

static void annotate(std::vector<Row>& rows, const std::set<std::string>& exact) {
    std::vector<Row*> ok_exact;
    for (auto& r : rows)
        if (exact.count(r.solver) && r.status == "ok") ok_exact.push_back(&r);
    bool have_ref = false, ref_feasible = false;
    double ref_e = 0;
    for (Row* r : ok_exact)
        if (r->feasible && *r->feasible && r->energy) {
            have_ref = ref_feasible = true;
            ref_e = *r->energy;
            break;
        }
    if (!have_ref && !ok_exact.empty()) have_ref = true;  // tất cả cùng vô nghiệm
    if (have_ref) {
        for (Row* r : ok_exact) {
            bool same = ref_feasible ? (r->feasible && *r->feasible && r->energy &&
                                        std::fabs(*r->energy - ref_e) <= REL_TOL * std::max(1.0, std::fabs(ref_e)))
                                     : (r->feasible && !*r->feasible);
            r->match_ref = same ? "ok" : "MISMATCH";
            if (!same)
                std::cerr << "CẢNH BÁO: " << r->file << ": " << r->solver
                          << " lệch năng lượng so với solver chính xác khác\n";
        }
    }
    if (have_ref && ref_feasible)
        for (auto& r : rows)
            if (HEURISTICS.count(r.solver) && r.status == "ok" && r.feasible && *r.feasible && r.energy &&
                *r.energy != 0)
                r.saved = (*r.energy - ref_e) / *r.energy * 100.0;
}

static std::vector<Row> bench_file(const Args& a, const std::string& path, const std::vector<std::string>& solvers,
                                   const std::set<std::string>& exact) {
    Row head = read_header(path);
    Opt load_rss;
    if (!a.load_solver.empty()) {
        RunResult r = run_once({a.bin, CLI_FLAG, a.load_solver, "--input", path}, a.timeout);
        if (r.status == "ok" && r.rc == 0) load_rss = r.rss_mb;
    }
    std::vector<Row> rows;
    for (const auto& solver : solvers) {
        for (int rep = 1; rep <= a.repeats; ++rep) {
            Row row = head;
            row.solver = solver;
            row.rep = rep;
            auto lim = a.max_n.find(solver);
            if (lim != a.max_n.end() && head.n && *head.n > lim->second) {
                row.status = "skipped";
                row.note = "n > max-n";
                rows.push_back(row);
                break;
            }
            RunResult r = run_once({a.bin, CLI_FLAG, solver, "--input", path}, a.timeout);
            row.wall_ms = std::round(r.wall_ms * 1000) / 1000;
            row.peak_rss = r.rss_mb;
            if (r.rss_mb) row.mem_src = mem_mode;
            if (r.rss_mb && load_rss) {
                row.load_rss = load_rss;
                row.net_rss = std::max(0.0, *r.rss_mb - *load_rss);
            }
            row.status = r.status;
            if (r.status == "ok") {
                if (r.rc == 3) {  // NotImplemented
                    row.status = "not_impl";
                    row.note = trim(r.err).substr(0, 200);
                    rows.push_back(row);
                    break;
                }
                std::optional<Json> res;
                try {
                    if (!trim(r.out).empty()) res = edgedp::parse_json(r.out);
                } catch (const std::exception&) {
                }
                if (r.rc != 0 || !res || res->type != Json::Obj) {
                    row.status = "error";
                    row.note = "rc=" + std::to_string(r.rc) + "; " + trim(r.err).substr(0, 200);
                } else {
                    if (const Json* f = res->get("feasible"))
                        if (f->type == Json::Bool) row.feasible = f->b;
                    row.energy = json_num(res->get("energy"));  // null -> nullopt
                    row.nodes_used = json_num(res->get("nodes_used"));
                    row.time_ms = json_num(res->get("time_ms"));
                    row.vcap = json_num(res->get("violated_cap"));
                    row.vlink = json_num(res->get("violated_link"));
                }
            }
            rows.push_back(row);
            if (r.status != "ok") break;  // timeout: không lặp lại
        }
    }
    annotate(rows, exact);
    return rows;
}

// ----------------------------------------------------------------------------- ghi CSV
static void write_row(std::ostream& os, const Row& r) {
    std::vector<std::string> c = {
        r.file, r.source, fnum(r.n, "%.0f"), fnum(r.m, "%.0f"), fnum(r.alpha, "%.15g"), fnum(r.beta, "%.15g"),
        fnum(r.seed, "%.0f"), r.solver, std::to_string(r.rep), r.status,
        r.feasible ? (*r.feasible ? "True" : "False") : "", fnum(r.energy, "%.15g"), fnum(r.nodes_used, "%.0f"),
        fnum(r.time_ms, "%.6g"), fnum(r.wall_ms, "%.3f"), fnum(r.peak_rss, "%.3f"), fnum(r.load_rss, "%.3f"),
        fnum(r.net_rss, "%.3f"), r.mem_src, fnum(r.vcap, "%.0f"), fnum(r.vlink, "%.0f"), r.match_ref,
        fnum(r.saved, "%.6f"), r.note};
    for (size_t i = 0; i < c.size(); ++i) os << (i ? "," : "") << csv_escape(c[i]);
    os << "\n";
}

static const char* FIELDS =
    "file,source,n,m,alpha,beta,seed,solver,rep,status,feasible,energy,nodes_used,time_ms,wall_ms,"
    "peak_rss_mb,load_rss_mb,net_rss_mb,mem_src,violated_cap,violated_link,match_ref,energy_saved_pct,note";

// ----------------------------------------------------------------------------- tổng hợp
static std::pair<std::string, std::string> mean_std(const std::vector<double>& v) {
    if (v.empty()) return {"", ""};
    double mean = 0;
    for (double x : v) mean += x;
    mean /= v.size();
    double sd = 0;
    if (v.size() > 1) {
        for (double x : v) sd += (x - mean) * (x - mean);
        sd = std::sqrt(sd / (v.size() - 1));
    }
    char a[64], b[64];
    std::snprintf(a, sizeof a, "%.6f", mean);
    std::snprintf(b, sizeof b, "%.6f", sd);
    return {a, b};
}

static void write_summary(const std::vector<Row>& rows, const std::string& path) {
    // Gộp các lần lặp của mỗi file (trung bình), rồi tính mean/std trên các seed.
    std::map<std::pair<std::string, std::string>, std::vector<const Row*>> per_file;
    for (auto& r : rows) per_file[{r.solver, r.file}].push_back(&r);
    using Key = std::tuple<std::string, std::string, std::string, std::string, std::string>;
    std::map<Key, std::vector<std::vector<const Row*>>> groups;
    for (auto& kv : per_file) {
        const Row& f = *kv.second[0];
        groups[Key{kv.first.first, fnum(f.n, "%.0f"), fnum(f.m, "%.0f"), fnum(f.alpha, "%.15g"),
                   fnum(f.beta, "%.15g")}]
            .push_back(kv.second);
    }
    auto avg = [](const std::vector<const Row*>& rs, Opt Row::*col) -> Opt {
        double s = 0;
        int k = 0;
        for (auto* r : rs)
            if (r->status == "ok" && (r->*col)) {
                s += *(r->*col);
                ++k;
            }
        return k ? Opt(s / k) : std::nullopt;
    };
    std::ofstream os(path);
    os << "solver,n,m,alpha,beta,files,ok,timeout,skipped,error,not_impl,time_ms_mean,time_ms_std,wall_ms_mean,"
          "wall_ms_std,peak_rss_mb_mean,peak_rss_mb_std,net_rss_mb_mean,net_rss_mb_std,energy_mean,energy_std,"
          "violation_rate,energy_saved_pct_mean,energy_saved_pct_std,mismatch\n";
    struct Col {
        Opt Row::*p;
    };
    const Col cols[] = {{&Row::time_ms}, {&Row::wall_ms}, {&Row::peak_rss}, {&Row::net_rss}, {&Row::energy}};
    auto stats = [&](const std::vector<std::vector<const Row*>>& files, Opt Row::*p) {
        std::vector<double> v;
        for (auto& fs_ : files)
            if (Opt a = avg(fs_, p)) v.push_back(*a);
        return mean_std(v);
    };
    for (auto& g : groups) {
        const auto& files = g.second;
        int cnt[5] = {0, 0, 0, 0, 0};  // ok, timeout, skipped, error, not_impl
        static const char* names[5] = {"ok", "timeout", "skipped", "error", "not_impl"};
        for (auto& fs_ : files)
            for (int i = 0; i < 5; ++i)
                if (fs_[0]->status == names[i]) ++cnt[i];
        os << std::get<0>(g.first) << "," << std::get<1>(g.first) << "," << std::get<2>(g.first) << ","
           << std::get<3>(g.first) << "," << std::get<4>(g.first) << "," << files.size();
        for (int i = 0; i < 5; ++i) os << "," << cnt[i];
        for (auto& c : cols) {
            auto ms = stats(files, c.p);
            os << "," << ms.first << "," << ms.second;
        }
        int nf = 0, feas = 0, mism = 0;
        for (auto& fs_ : files) {
            if (fs_[0]->status == "ok" && fs_[0]->feasible) {
                ++nf;
                feas += *fs_[0]->feasible ? 1 : 0;
            }
            for (auto* r : fs_) mism += r->match_ref == "MISMATCH";
        }
        char vr[64] = "";
        if (nf) std::snprintf(vr, sizeof vr, "%.6f", 1.0 - double(feas) / nf);
        auto sv = stats(files, &Row::saved);
        os << "," << vr << "," << sv.first << "," << sv.second << "," << mism << "\n";
    }
}

// ----------------------------------------------------------------------------- main
static void usage() {
    std::cerr << "Dùng: run_bench --bin <cli> [--solvers a,b] [--exact a,b] [--repeats N] [--timeout S]\n"
                 "        [--max-n SOLVER=N]... [--load-solver NAME] [--mem-mode auto|time|proc|none]\n"
                 "        [--out file.csv] [--summary file.csv] <inputs...>\n";
}

int main(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string k = argv[i];
        auto val = [&]() -> std::string {
            if (i + 1 >= argc) {
                usage();
                std::exit(2);
            }
            return argv[++i];
        };
        if (k == "--bin") a.bin = val();
        else if (k == "--solvers") a.solvers = val();
        else if (k == "--exact") a.exact = val();
        else if (k == "--repeats") a.repeats = std::atoi(val().c_str());
        else if (k == "--timeout") a.timeout = std::atof(val().c_str());
        else if (k == "--max-n") {
            std::string v = val();
            size_t eq = v.find('=');
            if (eq == std::string::npos) {
                usage();
                return 2;
            }
            a.max_n[alias(v.substr(0, eq))] = static_cast<long long>(std::atof(v.c_str() + eq + 1));
        } else if (k == "--load-solver") a.load_solver = val();
        else if (k == "--mem-mode") a.mem_mode = val();
        else if (k == "--out") a.out = val();
        else if (k == "--summary") a.summary = val();
        else if (!k.empty() && k[0] == '-' && k.size() > 1) {
            std::cerr << "Tham số không hợp lệ: " << k << "\n";
            usage();
            return 2;
        } else a.inputs.push_back(k);
    }
    if (a.bin.empty() || a.inputs.empty()) {
        usage();
        return 2;
    }
    if (!fs::is_regular_file(a.bin)) {
        std::cerr << "không thấy CLI: " << a.bin << " (build trước, hoặc sửa --bin)\n";
        return 2;
    }
    detect_mem_mode(a.mem_mode);
    std::cerr << "Đo bộ nhớ bằng: " << mem_mode << "\n";

    auto files = collect_inputs(a.inputs);
    if (files.empty()) {
        std::cerr << "không có file input nào\n";
        return 2;
    }
    std::vector<std::string> solvers;
    std::set<std::string> exact;
    for (auto& s : split(a.solvers, ',')) if (!s.empty()) solvers.push_back(alias(s));
    for (auto& s : split(a.exact, ',')) if (!s.empty()) exact.insert(alias(s));

    fs::path od = fs::path(a.out).parent_path();
    if (!od.empty()) fs::create_directories(od);
    std::vector<Row> all_rows;
    {
        std::ofstream os(a.out);
        os << FIELDS << "\n";
        for (size_t idx = 0; idx < files.size(); ++idx) {
            std::cerr << "[" << idx + 1 << "/" << files.size() << "] " << fs::path(files[idx]).filename().string()
                      << "\n";
            std::vector<Row> rows;
            try {
                rows = bench_file(a, files[idx], solvers, exact);
            } catch (const std::exception& e) {
                std::cerr << "Bỏ qua " << files[idx] << ": " << e.what() << "\n";
                continue;
            }
            for (auto& r : rows) write_row(os, r);
            os.flush();  // giữ kết quả dở dang nếu bị ngắt giữa chừng
            all_rows.insert(all_rows.end(), rows.begin(), rows.end());
        }
    }
    std::cerr << "Đã ghi " << all_rows.size() << " dòng -> " << a.out << "\n";
    if (!a.summary.empty()) {
        fs::path sd = fs::path(a.summary).parent_path();
        if (!sd.empty()) fs::create_directories(sd);
        write_summary(all_rows, a.summary);
        std::cerr << "Đã ghi tổng hợp -> " << a.summary << "\n";
    }
    int bad = 0;
    for (auto& r : all_rows) bad += r.match_ref == "MISMATCH";
    if (bad) {
        std::cerr << "CÓ " << bad << " dòng MISMATCH: các solver chính xác không cho cùng năng lượng.\n";
        return 3;
    }
    return 0;
}
