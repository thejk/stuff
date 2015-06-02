#include "common.hh"

#include <cstdarg>
#include <iostream>

#include "args.hh"

using namespace stuff;

namespace {

bool compare(const std::string& in, const std::vector<std::string>& out,
             va_list args) {
    for (const auto& arg : out) {
        const char* str = va_arg(args, const char*);
        if (!str) {
            std::cerr << "expected less arguments: " << in
                      << ", first extra: " << arg << std::endl;
            return false;
        }
        if (arg.compare(str) != 0) {
            std::cerr << "expected " << str << " in " << in
                      << ", got: " << arg << std::endl;
            return false;
        }
    }
    const char* str = va_arg(args, const char*);
    if (str) {
        std::cerr << "expected more arguments: " << in
                  << ", missing: " << str << std::endl;
        return false;
    }
    return true;
}

bool test(const char* in, ...) {
    std::vector<std::string> out;
    if (!Args::parse(in, &out, false)) {
        std::cerr << "expected success: " << in << std::endl;
        return false;
    }
    va_list args;
    va_start(args, in);
    bool ret = compare(in, out, args);
    va_end(args);
    return ret;
}

bool test_nice(const char* in, ...) {
    std::vector<std::string> out;
    if (!Args::parse(in, &out, true)) {
        std::cerr << "expected success: " << in << std::endl;
        return false;
    }
    va_list args;
    va_start(args, in);
    bool ret = compare(in, out, args);
    va_end(args);
    return ret;
}

bool test_fail(const std::string& in) {
    std::vector<std::string> out;
    if (Args::parse(in, &out, false)) {
        std::cerr << "expected fail: " << in << std::endl;
        return false;
    }
    return true;
}

}  // namespace

int main() {
    unsigned int ok = 0, tot = 0;

    tot++; if (test("", NULL)) ok++;
    tot++; if (test("foo", "foo", NULL)) ok++;
    tot++; if (test("hello world", "hello", "world", NULL)) ok++;
    tot++; if (test("\"hello world\"", "hello world", NULL)) ok++;
    tot++; if (test("'hello world'", "hello world", NULL)) ok++;
    tot++; if (test("a' b'", "a b", NULL)) ok++;
    tot++; if (test("a'b'ba", "abba", NULL)) ok++;
    tot++; if (test("a'b'b '' a", "abb", "", "a", NULL)) ok++;
    tot++; if (test("'\"'", "\"", NULL)) ok++;
    tot++; if (test("'\\\"'", "\"", NULL)) ok++;
    tot++; if (test("'\\\\'", "\\", NULL)) ok++;
    tot++; if (test("'\\''", "'", NULL)) ok++;
    tot++; if (test("'a\\'b'", "a'b", NULL)) ok++;
    tot++; if (test("a'\\''b", "a'b", NULL)) ok++;

    tot++; if (test_nice("a'b", "a'b", NULL)) ok++;
    tot++; if (test_nice("a'b foo", "a'b", "foo", NULL)) ok++;
    tot++; if (test_nice("'\\", "'\\", NULL)) ok++;
    tot++; if (test_nice("'\\'", "'\\'", NULL)) ok++;

    tot++; if (test_fail("'")) ok++;
    tot++; if (test_fail("'''")) ok++;
    tot++; if (test_fail("\"")) ok++;
    tot++; if (test_fail("'\\")) ok++;
    tot++; if (test_fail("'\\'")) ok++;

    std::cout << "OK " << ok << "/" << tot << std::endl;
    return ok == tot ? EXIT_SUCCESS : EXIT_FAILURE;
}
