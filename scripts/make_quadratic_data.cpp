#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "src/json.hpp"

namespace fs = std::filesystem;
using edgedp::Json;
using edgedp::parse_json;

static std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

int main() {
    std::string in_dir = "data/synthetic";
    std::string out_dir = "data/synthetic_quadratic";
    fs::create_directories(out_dir);

    std::string manifest_in = in_dir + "/manifest.csv";
    std::string manifest_out = out_dir + "/manifest.csv";

    std::ifstream mf(manifest_in);
    if (!mf) {
        std::cerr << "Không mở được " << manifest_in << "\n";
        return 1;
    }

    std::ofstream mf_out(manifest_out);
    std::string line;
    std::getline(mf, line);
    mf_out << line << "\n";

    int count = 0;
    while (std::getline(mf, line)) {
        if (line.empty()) continue;
        mf_out << line << "\n";

        std::stringstream ss(line);
        std::string filename;
        std::getline(ss, filename, ',');

        std::string json_text = read_file(in_dir + "/" + filename);
        Json root = parse_json(json_text);

        // Lấy w, d, m, n, beta
        int n = static_cast<int>(root.get("n")->num);
        int m = static_cast<int>(root.get("m")->num);
        long double w_sum = 0, d_sum = 0;
        const Json* w_arr = root.get("w");
        const Json* d_arr = root.get("d");
        for (const auto& x : w_arr->arr)
            w_sum += x.num;
        for (const auto& x : d_arr->arr)
            d_sum += x.num;

        long double w_bar = w_sum / m;
        long double d_bar = d_sum / n;

        const Json* meta = root.get("meta");
        long double beta = meta->get("beta")->num;

        // Quadratic: e_on = beta * w_bar^2, eps = 0.05 * e_on / d_bar
        long double e_on = beta * w_bar * w_bar;
        long double eps = 0.05 * e_on / d_bar;

        const Json* params = root.get("params");
        long double cap = params->get("cap")->num;
        long double link_cap = params->get("link_cap")->num;
        long double kappa = params->get("kappa_over_T2")->num;
        int seed = static_cast<int>(root.get("seed")->num);

        // Xuất file JSON quadratic
        std::ofstream out_f(out_dir + "/" + filename);
        out_f << std::setprecision(16);
        out_f << "{\"n\":" << n << ",\"m\":" << m << ",\"w\":[";
        for (size_t i = 0; i < w_arr->arr.size(); ++i) {
            if (i) out_f << ",";
            out_f << static_cast<long long>(w_arr->arr[i].num);
        }
        out_f << "],\"d\":[";
        for (size_t i = 0; i < d_arr->arr.size(); ++i) {
            if (i) out_f << ",";
            out_f << static_cast<long long>(d_arr->arr[i].num);
        }
        out_f << "],\"params\":{";
        out_f << "\"model\":\"quadratic\",";
        out_f << "\"quad_a\":1.0,";
        out_f << "\"quad_b\":0.0,";
        out_f << "\"kappa_over_T2\":" << kappa << ",";
        out_f << "\"e_on\":" << e_on << ",";
        out_f << "\"eps\":" << eps << ",";
        out_f << "\"cap\":" << cap << ",";
        out_f << "\"link_cap\":" << link_cap;
        out_f << "},\"seed\":" << seed << ",\"source\":\"synthetic_quadratic\",";
        out_f << "\"meta\":{";
        out_f << "\"alpha\":" << meta->get("alpha")->num << ",";
        out_f << "\"beta\":" << beta << ",";
        out_f << "\"alpha_eff\":" << meta->get("alpha_eff")->num << ",";
        out_f << "\"cap_adjusted\":" << (meta->get("cap_adjusted")->b ? "true" : "false") << ",";
        out_f << "\"w_bar\":" << w_bar;
        out_f << "}}\n";

        ++count;
        if (count % 50 == 0) { std::cout << "[" << count << "/300] " << filename << "\n"; }
    }
    std::cout << "Hoàn thành tạo " << count << " file quadratic!\n";
    return 0;
}
