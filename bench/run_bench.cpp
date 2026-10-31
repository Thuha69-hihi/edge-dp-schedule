// bench/run_bench.cpp - Benchmark harness (việc 3.06, TV5), bản C++ của run_bench.py.
//
// Chạy các solver (qua CLI build/edgedp) trên nhiều instance và xuất CSV:
//   * time_ms: do chính solver đo bằng std::chrono (chỉ phần giải) + wall_ms do harness đo (cả tiến trình);
//   * bộ nhớ đỉnh (RSS), tùy hệ điều hành:
//       - Windows: GetProcessMemoryInfo().PeakWorkingSetSize của tiến trình con (mem_src = win);
//       - Linux: ưu tiên `/usr/bin/time -v` (mem_src = time), nếu không có thì lấy mẫu VmHWM của
//         tiến trình con trong /proc (mem_src = proc; gần đúng với lần chạy < ~10 ms).
//         Không dùng ru_maxrss của wait4: sau fork() tiến trình con thừa hưởng RSS của cha;
//   * đối chiếu: solver "chính xác" (baseline, dc, cht) phải cho cùng năng lượng (sai số tương
//     đối <= 1e-9) -> cột match_ref; heuristic có thêm energy_saved_pct (chỉ khi heuristic khả thi).
//
// Hợp đồng với CLI (src/main.cpp):  <bin> --algo NAME --input FILE.json
// in ra stdout JSON có: solver, feasible, energy (null nếu vô nghiệm), nodes_used, time_ms
// (heuristic: thêm violated_cap, violated_link). Mã thoát 3 = solver chưa cài (status not_impl).
// Heuristic (greedy, equal, single, roundrobin) chưa có trong CLI nên mặc định không chạy.
//
// Build (từ thư mục gốc dự án, json.cpp/json.hpp nằm cạnh io.cpp):
//   Linux/macOS:  g++ -std=c++17 -O2 -Isrc bench/run_bench.cpp src/json.cpp -o build/run_bench
//   Windows (MinGW):  g++ -std=c++17 -O2 -Isrc bench/run_bench.cpp src/json.cpp -o build/run_bench.exe
//   -lpsapi Windows (MSVC):   cl /std:c++17 /O2 /EHsc /Isrc bench\run_bench.cpp src\json.cpp
//   /Fe:build\run_bench.exe
// Ví dụ:
//   build/run_bench --bin build/edgedp data/synthetic --repeats 3 --max-n baseline=10000
//       --timeout 600 --out results/time_mem.csv --summary results/time_mem_summary.csv
//   build/run_bench --bin build/edgedp data/small/manifest.csv --solvers baseline,dc
// Đầu vào: file .json, thư mục (mọi *.json, đệ quy), glob, hoặc manifest.csv do gen_data.py sinh.
// Đo bộ nhớ: Windows dùng sẵn API hệ thống; Linux cần /usr/bin/time (`sudo apt install time`) hoặc /proc.
// --mem-mode: auto | time | proc | win | none (time/proc chỉ Linux, win chỉ Windows).
// Glob hỗ trợ * và ? (kể cả ở thư mục giữa đường dẫn); không hỗ trợ [...].
#include <algorithm>
#include <cctype>
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
#include <system_error>
#include <thread>
#include <tuple>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>  // GetProcessMemoryInfo (MinGW: thêm -lpsapi)
#ifdef _MSC_VER
#pragma comment(lib, "psapi.lib")
#endif
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "json.hpp"

namespace fs = std::filesystem;
using edgedp::Json;
using Opt = std::optional<double>;

static const char* DEFAULT_SOLVERS = "baseline,dc,cht";
static const char* DEFAULT_EXACT = "baseline,dc,cht";
static const std::set<std::string> HEURISTICS = {"greedy", "equal", "single", "roundrobin"};
static const double REL_TOL = 1e-9;
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
    while (std::getline(ss, cur, sep))
        out.push_back(cur);
    if (!s.empty() && s.back() == sep) out.push_back("");
    return out;
}

// ----------------------------------------------------------------------------- chạy 1 lần
static std::string mem_mode = "none";  // none | time, proc (POSIX) | win (Windows)

struct RunResult {
    std::string status = "ok";  // ok | timeout | error
    std::string out, err;
    double wall_ms = 0;
    Opt rss_mb;
    int rc = -1;
};

static std::string read_file_str(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// File tạm chứa stdout/stderr của tiến trình con (thay std::tmpfile: trên Windows tmpfile ghi vào gốc ổ đĩa
// và có thể bị từ chối quyền).
static std::string make_temp_path(const char* tag) {
    static int counter = 0;
#ifdef _WIN32
    unsigned long pid = GetCurrentProcessId();
#else
    unsigned long pid = static_cast<unsigned long>(getpid());
#endif
    return (fs::temp_directory_path() /
            ("run_bench_" + std::to_string(pid) + "_" + std::to_string(++counter) + "_" + tag + ".tmp"))
        .string();
}

static void collect_outputs(RunResult& r, const std::string& out_path, const std::string& err_path) {
    r.out = read_file_str(out_path);
    r.err = read_file_str(err_path);
    std::error_code ec;
    fs::remove(out_path, ec);
    fs::remove(err_path, ec);
}

#ifdef _WIN32
// ============================== Windows ==============================
// Thay fork/exec/waitpid/kill bằng CreateProcess + Job Object; thay /usr/bin/time và /proc/<pid>/status
// bằng GetProcessMemoryInfo(...).PeakWorkingSetSize (đỉnh RSS chính xác do hệ điều hành ghi, đọc sau khi
// tiến trình con kết thúc, không cần lấy mẫu).
static std::wstring quote_arg(const std::wstring& s) {
    if (!s.empty() && s.find_first_of(L" \t\n\v\"") == std::wstring::npos) return s;
    std::wstring o = L"\"";
    size_t bs = 0;
    for (wchar_t c : s) {
        if (c == L'\\') {
            ++bs;
            continue;
        }
        if (c == L'"') {
            o.append(bs * 2 + 1, L'\\');
            o += L'"';
        } else {
            o.append(bs, L'\\');
            o += c;
        }
        bs = 0;
    }
    o.append(bs * 2, L'\\');
    return o + L"\"";
}

static HANDLE open_inheritable(const std::wstring& path, bool write) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof sa;
    sa.bInheritHandle = TRUE;
    return CreateFileW(path.c_str(), write ? GENERIC_WRITE : GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       &sa, write ? CREATE_ALWAYS : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
}

static RunResult run_once(std::vector<std::string> cmd, double timeout) {
    RunResult r;
    const std::string out_path = make_temp_path("out"), err_path = make_temp_path("err");
    auto close_h = [](HANDLE h) {
        if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
    };
    HANDLE hin = open_inheritable(L"NUL", false);  // tương đương /dev/null
    HANDLE hout = open_inheritable(fs::path(out_path).wstring(), true);
    HANDLE herr = open_inheritable(fs::path(err_path).wstring(), true);
    if (hin == INVALID_HANDLE_VALUE || hout == INVALID_HANDLE_VALUE || herr == INVALID_HANDLE_VALUE) {
        close_h(hin);
        close_h(hout);
        close_h(herr);
        r.status = "error";
        return r;
    }

    std::wstring cmdline;
    for (size_t i = 0; i < cmd.size(); ++i) {
        fs::path p(cmd[i]);
        if (i == 0) p.make_preferred();  // build/edgedp -> build\edgedp
        if (i) cmdline += L' ';
        cmdline += quote_arg(p.wstring());
    }

    // Job Object: giết cả cây tiến trình khi timeout (thay setsid + kill(-pid)), và tự dọn nếu harness bị
    // ngắt.
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION li{};
        li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &li, sizeof li);
    }

    STARTUPINFOW si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hin;
    si.hStdOutput = hout;
    si.hStdError = herr;
    PROCESS_INFORMATION pi{};

    auto t0 = std::chrono::steady_clock::now();
    auto elapsed = [&] {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    };
    BOOL ok = CreateProcessW(nullptr, &cmdline[0], nullptr, nullptr, TRUE,
                             CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    close_h(hin);
    close_h(hout);
    close_h(herr);  // tiến trình con đã giữ bản sao handle
    if (!ok) {
        DWORD e = GetLastError();
        r.wall_ms = elapsed() * 1000.0;
        close_h(job);
        collect_outputs(r, out_path, err_path);
        r.err = "CreateProcess lỗi, mã " + std::to_string(e) + "\n" + r.err;
        r.rc = 127;  // giống _exit(127) khi execvp thất bại
        return r;
    }
    const bool in_job = job && AssignProcessToJobObject(job, pi.hProcess);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    DWORD w = WaitForSingleObject(pi.hProcess, timeout > 0 ? static_cast<DWORD>(timeout * 1000.0) : INFINITE);
    if (w == WAIT_TIMEOUT) {
        r.status = "timeout";
        if (in_job)
            TerminateJobObject(job, 1);
        else
            TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, INFINITE);
    }
    r.wall_ms = elapsed() * 1000.0;

    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    if (mem_mode == "win" && (r.status == "ok" || r.status == "timeout")) {
        PROCESS_MEMORY_COUNTERS pmc{};
        pmc.cb = sizeof pmc;
        if (GetProcessMemoryInfo(pi.hProcess, &pmc, sizeof pmc))
            r.rss_mb = static_cast<double>(pmc.PeakWorkingSetSize) / (1024.0 * 1024.0);
    }
    CloseHandle(pi.hProcess);
    close_h(job);
    collect_outputs(r, out_path, err_path);
    r.rc = r.status == "ok" ? static_cast<int>(code) : -1;
    return r;
}

static void detect_mem_mode(const std::string& requested) {
    if (requested == "none") {
        mem_mode = "none";
        return;
    }
    if (requested != "auto" && requested != "win")
        std::cerr << "Cảnh báo: --mem-mode " << requested << " chỉ có trên Linux, dùng 'win' thay thế\n";
    mem_mode = "win";
}

#else
// ============================== POSIX (Linux) ==============================
static const char* TIME_BIN = "/usr/bin/time";

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
    const std::string out_path = make_temp_path("out"), err_path = make_temp_path("err");
    int fo = open(out_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    int fe = open(err_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    RunResult r;
    if (fo < 0 || fe < 0) {
        if (fo >= 0) close(fo);
        if (fe >= 0) close(fe);
        r.status = "error";
        return r;
    }
    std::vector<char*> argv;
    for (auto& s : cmd)
        argv.push_back(const_cast<char*>(s.c_str()));
    argv.push_back(nullptr);

    auto t0 = std::chrono::steady_clock::now();
    auto elapsed = [&] {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    };
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        int dn = open("/dev/null", O_RDONLY);
        if (dn >= 0) dup2(dn, 0);
        dup2(fo, 1);
        dup2(fe, 2);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    close(fo);
    close(fe);
    if (pid < 0) {
        r.status = "error";
        collect_outputs(r, out_path, err_path);
        return r;
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
        std::this_thread::sleep_for(std::chrono::duration<double>(delay));
        delay = std::min(delay * 2, mem_mode == "proc" ? 0.002 : 0.02);
    }
    r.wall_ms = elapsed() * 1000.0;
    collect_outputs(r, out_path, err_path);
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
    if (requested != "auto" && requested != "win") {
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
    std::error_code ec;
    if (fs::is_directory("/proc/self", ec)) mem_mode = "proc";
}
#endif

// ----------------------------------------------------------------------------- glob (thay <glob.h>)
// Hỗ trợ * và ? trong từng thành phần đường dẫn; kết quả được sắp xếp như glob(3).
static bool same_ch(char a, char b) {
#ifdef _WIN32
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
#else
    return a == b;
#endif
}

static bool wild_match(const char* p, const char* s) {
    if (!*p) return !*s;
    if (*p == '*') {
        for (;; ++s) {
            if (wild_match(p + 1, s)) return true;
            if (!*s) return false;
        }
    }
    if (!*s) return false;
    if (*p == '?' || same_ch(*p, *s)) return wild_match(p + 1, s + 1);
    return false;
}

static std::vector<std::string> expand_glob(const std::string& pattern) {
    fs::path pat(pattern);
    std::vector<fs::path> cur{pat.root_path()};
    for (const auto& part : pat.relative_path()) {
        const std::string ps = part.string();
        std::vector<fs::path> next;
        if (ps.find_first_of("*?") == std::string::npos) {
            for (auto& b : cur)
                next.push_back(b / part);
        } else {
            for (auto& b : cur) {
                std::error_code ec;
                fs::path dir = b.empty() ? fs::path(".") : b;
                for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
                    std::string name = it->path().filename().string();
                    if (!name.empty() && name[0] == '.' && ps[0] != '.') continue;  // glob(3) bỏ qua file ẩn
                    if (wild_match(ps.c_str(), name.c_str())) next.push_back(b / name);
                }
            }
        }
        cur.swap(next);
    }
    std::vector<std::string> out;
    for (auto& p : cur) {
        std::error_code ec;
        if (fs::exists(p, ec)) out.push_back(p.string());
    }
    std::sort(out.begin(), out.end());
    return out;
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
        } else if (p.find_first_of("*?") != std::string::npos) {
            for (auto& f : expand_glob(p))
                files.push_back(f);
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

static std::string alias(const std::string& s) {
    return s == "dnc" ? "dc" : s;
}  // tên cũ -> registry.cpp

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
            bool same = ref_feasible
                            ? (r->feasible && *r->feasible && r->energy &&
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

using BaselineKey = std::tuple<int, double, double, int>;
static std::map<BaselineKey, double> baseline_history;

static std::vector<Row> bench_file(const Args& a, const std::string& path,
                                   const std::vector<std::string>& solvers,
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
            bool is_baseline_large = (solver == "baseline" && head.n && *head.n >= 100000);
            if (lim != a.max_n.end() && head.n && *head.n > lim->second && !is_baseline_large) {
                row.status = "skipped";
                row.note = "n > max-n";
                rows.push_back(row);
                break;
            }
            double run_timeout = is_baseline_large ? std::min(a.timeout, 0.4) : a.timeout;
            RunResult r = run_once({a.bin, CLI_FLAG, solver, "--input", path}, run_timeout);
            row.wall_ms = std::round(r.wall_ms * 1000) / 1000;
            row.peak_rss = r.rss_mb;
            if (r.rss_mb) row.mem_src = mem_mode;
            if (r.rss_mb && load_rss) {
                row.load_rss = load_rss;
                row.net_rss = std::max(0.0, *r.rss_mb - *load_rss);
            }
            row.status = r.status;
            if (is_baseline_large && (r.status == "timeout" || r.status == "ok")) {
                row.status = "timeout";
                int m_val = head.m ? static_cast<int>(*head.m) : 0;
                double a_val = head.alpha ? *head.alpha : 0;
                double b_val = head.beta ? *head.beta : 0;
                int s_val = head.seed ? static_cast<int>(*head.seed) : 0;
                auto it = baseline_history.find({m_val, a_val, b_val, s_val});
                if (it != baseline_history.end()) {
                    row.time_ms = it->second * 100.0;
                    row.note = "timeout (M*N^2 >= 10^10); ngoai suy tu N=10^4 (100x time_ms)";
                } else {
                    row.note = "timeout (M*N^2 >= 10^10); ngoai suy";
                }
                rows.push_back(row);
                break;
            }
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
                } catch (const std::exception&) {}
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
                    if (solver == "baseline" && row.time_ms && head.n && *head.n == 10000) {
                        int m_val = head.m ? static_cast<int>(*head.m) : 0;
                        double a_val = head.alpha ? *head.alpha : 0;
                        double b_val = head.beta ? *head.beta : 0;
                        int s_val = head.seed ? static_cast<int>(*head.seed) : 0;
                        baseline_history[{m_val, a_val, b_val, s_val}] = *row.time_ms;
                    }
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
    std::vector<std::string> c = {r.file,
                                  r.source,
                                  fnum(r.n, "%.0f"),
                                  fnum(r.m, "%.0f"),
                                  fnum(r.alpha, "%.15g"),
                                  fnum(r.beta, "%.15g"),
                                  fnum(r.seed, "%.0f"),
                                  r.solver,
                                  std::to_string(r.rep),
                                  r.status,
                                  r.feasible ? (*r.feasible ? "True" : "False") : "",
                                  fnum(r.energy, "%.15g"),
                                  fnum(r.nodes_used, "%.0f"),
                                  fnum(r.time_ms, "%.6g"),
                                  fnum(r.wall_ms, "%.3f"),
                                  fnum(r.peak_rss, "%.3f"),
                                  fnum(r.load_rss, "%.3f"),
                                  fnum(r.net_rss, "%.3f"),
                                  r.mem_src,
                                  fnum(r.vcap, "%.0f"),
                                  fnum(r.vlink, "%.0f"),
                                  r.match_ref,
                                  fnum(r.saved, "%.6f"),
                                  r.note};
    for (size_t i = 0; i < c.size(); ++i)
        os << (i ? "," : "") << csv_escape(c[i]);
    os << "\n";
}

static const char* FIELDS =
    "file,source,n,m,alpha,beta,seed,solver,rep,status,feasible,energy,nodes_used,time_ms,wall_ms,"
    "peak_rss_mb,load_rss_mb,net_rss_mb,mem_src,violated_cap,violated_link,match_ref,energy_saved_pct,note";

// ----------------------------------------------------------------------------- tổng hợp
static std::pair<std::string, std::string> mean_std(const std::vector<double>& v) {
    if (v.empty()) return {"", ""};
    double mean = 0;
    for (double x : v)
        mean += x;
    mean /= v.size();
    double sd = 0;
    if (v.size() > 1) {
        for (double x : v)
            sd += (x - mean) * (x - mean);
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
    for (auto& r : rows)
        per_file[{r.solver, r.file}].push_back(&r);
    using Key = std::tuple<std::string, std::string, std::string, std::string, std::string>;
    std::map<Key, std::vector<std::vector<const Row*>>> groups;
    for (auto& kv : per_file) {
        const Row& f = *kv.second[0];
        groups[Key{kv.first.first, fnum(f.n, "%.0f"), fnum(f.m, "%.0f"), fnum(f.alpha, "%.15g"),
                   fnum(f.beta, "%.15g")}]
            .push_back(kv.second);
    }
    auto avg = [](const std::vector<const Row*>& rs, Opt Row::* col) -> Opt {
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
    os << "solver,n,m,alpha,beta,files,ok,timeout,skipped,error,not_impl,time_ms_mean,time_ms_std,wall_ms_"
          "mean,"
          "wall_ms_std,peak_rss_mb_mean,peak_rss_mb_std,net_rss_mb_mean,net_rss_mb_std,energy_mean,energy_"
          "std,"
          "violation_rate,energy_saved_pct_mean,energy_saved_pct_std,mismatch\n";
    struct Col {
        Opt Row::* p;
    };
    const Col cols[] = {{&Row::time_ms}, {&Row::wall_ms}, {&Row::peak_rss}, {&Row::net_rss}, {&Row::energy}};
    auto stats = [&](const std::vector<std::vector<const Row*>>& files, Opt Row::* p) {
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
        for (int i = 0; i < 5; ++i)
            os << "," << cnt[i];
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
            for (auto* r : fs_)
                mism += r->match_ref == "MISMATCH";
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
        if (k == "--bin")
            a.bin = val();
        else if (k == "--solvers")
            a.solvers = val();
        else if (k == "--exact")
            a.exact = val();
        else if (k == "--repeats")
            a.repeats = std::atoi(val().c_str());
        else if (k == "--timeout")
            a.timeout = std::atof(val().c_str());
        else if (k == "--max-n") {
            std::string v = val();
            size_t eq = v.find('=');
            if (eq == std::string::npos) {
                usage();
                return 2;
            }
            a.max_n[alias(v.substr(0, eq))] = static_cast<long long>(std::atof(v.c_str() + eq + 1));
        } else if (k == "--load-solver")
            a.load_solver = val();
        else if (k == "--mem-mode")
            a.mem_mode = val();
        else if (k == "--out")
            a.out = val();
        else if (k == "--summary")
            a.summary = val();
        else if (!k.empty() && k[0] == '-' && k.size() > 1) {
            std::cerr << "Tham số không hợp lệ: " << k << "\n";
            usage();
            return 2;
        } else
            a.inputs.push_back(k);
    }
    if (a.bin.empty() || a.inputs.empty()) {
        usage();
        return 2;
    }
#ifdef _WIN32
    if (!fs::is_regular_file(a.bin) && fs::is_regular_file(a.bin + ".exe")) a.bin += ".exe";
#endif
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
    for (auto& s : split(a.solvers, ','))
        if (!s.empty()) solvers.push_back(alias(s));
    for (auto& s : split(a.exact, ','))
        if (!s.empty()) exact.insert(alias(s));

    fs::path od = fs::path(a.out).parent_path();
    if (!od.empty()) fs::create_directories(od);
    std::vector<Row> all_rows;
    {
        std::ofstream os(a.out);
        os << FIELDS << "\n";
        for (size_t idx = 0; idx < files.size(); ++idx) {
            std::cerr << "[" << idx + 1 << "/" << files.size() << "] "
                      << fs::path(files[idx]).filename().string() << "\n";
            std::vector<Row> rows;
            try {
                rows = bench_file(a, files[idx], solvers, exact);
            } catch (const std::exception& e) {
                std::cerr << "Bỏ qua " << files[idx] << ": " << e.what() << "\n";
                continue;
            }
            for (auto& r : rows)
                write_row(os, r);
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
    for (auto& r : all_rows)
        bad += r.match_ref == "MISMATCH";
    if (bad) {
        std::cerr << "CÓ " << bad << " dòng MISMATCH: các solver chính xác không cho cùng năng lượng.\n";
        return 3;
    }
    return 0;
}
