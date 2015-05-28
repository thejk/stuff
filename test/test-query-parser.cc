#include "common.hh"

#include <cstdarg>
#include <cstdlib>
#include <iostream>

#include "query_parser.hh"

using namespace stuff;

namespace {

bool test(const char* in, ...) {
    std::map<std::string, std::string> tmp;
    if (!QueryParser::parse(in, &tmp)) {
        std::cerr << "expected success: " << in << std::endl;
        return false;
    }
    va_list args;
    va_start(args, in);
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
    std::map<std::string, std::string> tmp;
    if (!QueryParser::parse(in, &tmp)) return true;
    std::cerr << "expected fail: " << in << std::endl;
    return false;
}

}  // namespace

int main() {
    unsigned int ok = 0, tot = 0;

    tot++; if (test("", NULL)) ok++;
    tot++; if (test("key=value", "key", "value", NULL)) ok++;
    tot++; if (test("?key=value", "key", "value", NULL)) ok++;
    tot++; if (test("?key=value&", "key", "value", NULL)) ok++;
    tot++; if (test("key=value&foo=bar",
                    "key", "value", "foo", "bar", NULL)) ok++;
    tot++; if (test("key=value&key=bar", "key", "bar", NULL)) ok++;
    tot++; if (test("key=", "key", "", NULL)) ok++;
    tot++; if (test("key=%c3%A5", "key", "\xc3\xa5", NULL)) ok++;
    tot++; if (test("key=foo%20bar", "key", "foo bar", NULL)) ok++;
    tot++; if (test("key=foo+bar", "key", "foo bar", NULL)) ok++;
    tot++;
    if (test("first=this+is+a+field&second=was+it+clear+%28already%29%3F",
             "first", "this is a field",
             "second", "was it clear (already)?",
             NULL)) ok++;
    tot++; if (test_fail("=value")) ok++;
    tot++; if (test_fail("=")) ok++;
    tot++; if (test_fail("\xc3\xa5")) ok++;
    tot++; if (test_fail("&&")) ok++;
    tot++; if (test_fail("key=&&")) ok++;
    tot++; if (test_fail("==")) ok++;
    tot++; if (test_fail("=&=")) ok++;
    tot++; if (test_fail("%")) ok++;
    tot++; if (test_fail("%A")) ok++;
    tot++; if (test_fail("%1z")) ok++;

    std::cout << "OK " << ok << "/" << tot << std::endl;
    return ok == tot ? EXIT_SUCCESS : EXIT_FAILURE;
}
