#include "common.hh"

#include <cstdarg>
#include <cstdlib>
#include <iostream>

#include "header_parser.hh"

using namespace stuff;

namespace {

bool test(const char* in, const char* token, ...) {
    std::string tmp_token;
    std::map<std::string, std::string> tmp;
    if (!HeaderParser::parse(in, &tmp_token, &tmp)) {
        std::cerr << "expected success: " << in << std::endl;
        return false;
    }
    if (tmp_token.compare(token) != 0) {
        std::cerr << "expected token not to be " << tmp_token << " in: "
                  << in << std::endl;
        return false;
    }
    va_list args;
    va_start(args, token);
    while (true) {
        const char* key = va_arg(args, const char*);
        if (!key) break;
        const char* value = va_arg(args, const char*);
        auto it = tmp.find(key);
        if (it == tmp.end()) {
            std::cerr << "expected a value for " << key
                      << " in: " << in << std::endl;
            return false;
        }
        if (it->second.compare(value) != 0) {
            std::cerr << "expected value for " << key << " not to be "
                      << it->second << " in: " << in << std::endl;
            return false;
        }
        tmp.erase(it);
    }
    va_end(args);
    for (auto& pair : tmp) {
        std::cerr << "unexpected value for " << pair.first << ": "
                  << pair.second << " in: " << in << std::endl;
        return false;
    }
    return true;
}

bool test_fail(const std::string& in) {
    std::string token;
    if (!HeaderParser::parse(in, &token, nullptr)) return true;
    std::cerr << "expected fail: " << in << std::endl;
    return false;
}

}  // namespace

int main() {
    unsigned int ok = 0, tot = 0;

    tot++; if (test("text/html", "text/html", NULL)) ok++;
    tot++; if (test("text/html;charset=utf-8",
                    "text/html", "charset", "utf-8", NULL)) ok++;
    tot++; if (test("text/html;charset=UTF-8",
                    "text/html", "charset", "utf-8", NULL)) ok++;
    tot++; if (test("Text/HTML;Charset=\"utf-8\"",
                    "text/html", "charset", "utf-8", NULL)) ok++;
    tot++; if (test("text/html; charset=\"utf-8\"",
                    "text/html", "charset", "utf-8", NULL)) ok++;
    tot++; if (test("a/b; key=\"\\\"\"", "a/b", "key", "\"", NULL)) ok++;
    tot++; if (test_fail("")) ok++;
    tot++; if (test_fail("text/html;charset = utf-8")) ok++;
    tot++; if (test_fail("a/b;key=\"")) ok++;
    tot++; if (test_fail("a/b;key=\"\\")) ok++;
    tot++; if (test_fail("a/b;key=\"\\\"")) ok++;

    std::cout << "OK " << ok << "/" << tot << std::endl;
    return ok == tot ? EXIT_SUCCESS : EXIT_FAILURE;
}
