#include "json.hpp"

#include <cctype>
#include <cstdlib>
#include <stdexcept>

namespace edgedp {

const Json* Json::get(const std::string& key) const {
    if (type != Obj) return nullptr;
    for (const auto& kv : obj)
        if (kv.first == key) return &kv.second;
    return nullptr;
}

namespace {

struct Parser {
    const std::string& s;
    size_t p = 0;

    explicit Parser(const std::string& text) : s(text) {}

    [[noreturn]] void fail(const std::string& msg) const {
        throw std::runtime_error("JSON loi o vi tri " + std::to_string(p) + ": " + msg);
    }
    void ws() { while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) ++p; }
    char peek() { ws(); if (p >= s.size()) fail("het du lieu"); return s[p]; }
    void expect(char c) { if (peek() != c) fail(std::string("can '") + c + "'"); ++p; }
    bool match(const char* lit) {
        size_t n = std::char_traits<char>::length(lit);
        if (s.compare(p, n, lit) == 0) { p += n; return true; }
        return false;
    }

    std::string string_lit() {
        expect('"');
        std::string out;
        while (p < s.size() && s[p] != '"') {
            char c = s[p++];
            if (c == '\\') {
                if (p >= s.size()) fail("escape do dang");
                char e = s[p++];
                switch (e) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'u': p += 4; out += '?'; break;  // không cần unicode cho dữ liệu của nhóm
                    default: out += e;                     // \" \\ \/
                }
            } else {
                out += c;
            }
        }
        if (p >= s.size()) fail("chuoi khong dong");
        ++p;
        return out;
    }

    Json value() {
        Json j;
        char c = peek();
        if (c == '{') {
            ++p; j.type = Json::Obj;
            if (peek() == '}') { ++p; return j; }
            for (;;) {
                std::string key = string_lit();
                expect(':');
                j.obj.emplace_back(key, value());
                if (peek() == ',') { ++p; continue; }
                expect('}');
                break;
            }
        } else if (c == '[') {
            ++p; j.type = Json::Arr;
            if (peek() == ']') { ++p; return j; }
            for (;;) {
                j.arr.push_back(value());
                if (peek() == ',') { ++p; continue; }
                expect(']');
                break;
            }
        } else if (c == '"') {
            j.type = Json::Str; j.str = string_lit();
        } else if (match("true")) { j.type = Json::Bool; j.b = true; }
        else if (match("false")) { j.type = Json::Bool; j.b = false; }
        else if (match("null")) { j.type = Json::Null; }
        else {
            const char* begin = s.c_str() + p;
            char* end = nullptr;
            long double v = std::strtold(begin, &end);
            if (end == begin) fail("gia tri khong hop le");
            p += static_cast<size_t>(end - begin);
            j.type = Json::Num; j.num = v;
        }
        return j;
    }
};

}  // namespace

Json parse_json(const std::string& text) {
    Parser ps(text);
    Json j = ps.value();
    ps.ws();
    if (ps.p != text.size()) ps.fail("con du lieu thua sau gia tri JSON");
    return j;
}

}  // namespace edgedp
