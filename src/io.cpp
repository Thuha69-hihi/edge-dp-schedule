#include "io.hpp"

#include <cmath>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>

#include "json.hpp"

namespace edgedp {

namespace {

const Json& need(const Json& obj, const char* key, Json::Type type) {
    const Json* p = obj.get(key);
    if (!p) throw std::runtime_error(std::string("thieu truong: ") + key);
    if (p->type != type) throw std::runtime_error(std::string("sai kieu o truong: ") + key);
    return *p;
}

std::vector<ll> int_array(const Json& obj, const char* key) {
    const Json& a = need(obj, key, Json::Arr);
    std::vector<ll> out;
    out.reserve(a.arr.size());
    for (const auto& x : a.arr) {
        if (x.type != Json::Num) throw std::runtime_error(std::string("phan tu khong phai so trong ") + key);
        if (x.num != std::floor(x.num)) throw std::runtime_error(std::string("can so nguyen trong ") + key);
        if (x.num < 0) throw std::runtime_error(std::string("khong duoc am trong ") + key + " (w_i, d_i >= 0)");
        out.push_back(static_cast<ll>(x.num));
    }
    return out;
}

std::string num(ld v) {
    std::ostringstream os;
    os << std::setprecision(15) << static_cast<double>(v);
    return os.str();
}

}  // namespace

Instance parse_instance_json(const std::string& text) {
    Json root = parse_json(text);
    if (root.type != Json::Obj) throw std::runtime_error("goc JSON phai la object");

    Instance in;
    in.n = static_cast<int>(need(root, "n", Json::Num).num);
    in.m = static_cast<int>(need(root, "m", Json::Num).num);
    in.w = int_array(root, "w");
    in.d = int_array(root, "d");
    if (in.n < 0 || in.m < 0) throw std::runtime_error("n, m phai >= 0");
    if (static_cast<int>(in.w.size()) != in.n) throw std::runtime_error("w phai co dung n phan tu");
    if (static_cast<int>(in.d.size()) != in.n) throw std::runtime_error("d phai co dung n phan tu (d_0..d_{n-1})");

    const Json& p = need(root, "params", Json::Obj);
    in.params.kappa_over_T2 = need(p, "kappa_over_T2", Json::Num).num;
    in.params.e_on = need(p, "e_on", Json::Num).num;
    in.params.eps = need(p, "eps", Json::Num).num;
    in.params.cap = need(p, "cap", Json::Num).num;
    in.params.link_cap = need(p, "link_cap", Json::Num).num;

    // Mo hinh nang luong (tuy chon, mac dinh "cubic"). Tim "model" trong params truoc, roi o goc.
    // Voi "quadratic": E_exec(W) = quad_a*W^2 + quad_b*W; thieu quad_a thi lay kappa_over_T2, thieu quad_b thi 0.
    std::string model = "cubic";
    const Json* mj = p.get("model");
    if (!mj) mj = root.get("model");
    if (mj) {
        if (mj->type != Json::Str) throw std::runtime_error("sai kieu o truong: model (can chuoi)");
        model = mj->str;
    }
    if (model == "cubic") {
        in.params.model = EnergyModel::Cubic;
    } else if (model == "quadratic") {
        in.params.model = EnergyModel::Quadratic;
        in.params.quad_a = in.params.kappa_over_T2;
        in.params.quad_b = 0;
        if (const Json* qa = p.get("quad_a")) {
            if (qa->type != Json::Num) throw std::runtime_error("sai kieu o truong: quad_a");
            in.params.quad_a = qa->num;
        }
        if (const Json* qb = p.get("quad_b")) {
            if (qb->type != Json::Num) throw std::runtime_error("sai kieu o truong: quad_b");
            in.params.quad_b = qb->num;
        }
        if (in.params.quad_a < 0) throw std::runtime_error("quad_a phai >= 0 (ham nang luong loi)");
    } else {
        throw std::runtime_error("model khong hop le (cubic | quadratic): " + model);
    }

    if (const Json* s = root.get("seed")) if (s->type == Json::Num) in.seed = static_cast<ll>(s->num);
    if (const Json* s = root.get("source")) if (s->type == Json::Str) in.source = s->str;
    return in;
}

Instance read_instance_json(std::istream& is) {
    std::string text((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    return parse_instance_json(text);
}

std::string solution_to_json(const Solution& s) {
    std::ostringstream os;
    os << "{\n";
    os << "  \"feasible\": " << (s.feasible ? "true" : "false") << ",\n";
    os << "  \"energy\": " << (s.feasible ? num(s.energy) : "null") << ",\n";
    os << "  \"nodes_used\": " << s.nodes_used << ",\n";
    os << "  \"cuts\": [";
    for (size_t i = 0; i < s.cuts.size(); ++i) os << (i ? ", " : "") << s.cuts[i];
    os << "],\n";
    os << "  \"segments\": [";
    for (size_t i = 0; i < s.segments.size(); ++i) {
        const auto& g = s.segments[i];
        os << (i ? ",\n    " : "\n    ") << "{\"from\": " << g.from << ", \"to\": " << g.to
           << ", \"work\": " << g.work << ", \"cost\": " << num(g.cost) << "}";
    }
    os << (s.segments.empty() ? "]" : "\n  ]") << ",\n";
    os << "  \"solver\": \"" << s.solver << "\",\n";
    os << "  \"time_ms\": " << std::setprecision(6) << s.time_ms << "\n";
    os << "}\n";
    return os.str();
}

}  // namespace edgedp

