#include "common.hh"

#include <iostream>

#include "json.hh"

using namespace stuff;

namespace {

bool test_equal(const std::string& test, const std::string& value,
                const std::string& expected) {
    if (value == expected) return true;
    std::cerr << test << ": got '" << value <<
        "' expected '" << expected << "'" << std::endl;
    return false;
}

bool test_equal(const std::string& test, const std::string& value,
                const std::string& expected1,
                const std::string& expected2) {
    if (value == expected1 || value == expected2) return true;
    std::cerr << test << ": got '" << value <<
        "' expected '" << expected1 << "' or '" <<
        expected2 << "'" << std::endl;
    return false;
}

bool test_simple() {
    auto obj = JsonObject::create();
    if (!test_equal("simple", obj->str(), "{}"))
        return false;
    obj->put("foo", "bar");
    obj->put("value", static_cast<int64_t>(12));
    if (!test_equal("simple", obj->str(),
                    "{\"foo\":\"bar\",\"value\":12}",
                    "{\"value\":12,\"foo\":\"bar\"}"))
        return false;
    auto arr = JsonArray::create();
    if (!test_equal("simple", arr->str(), "[]"))
        return false;
    arr->put(0, static_cast<int64_t>(42));
    if (!test_equal("simple", arr->str(), "[42]"))
        return false;
    arr->put(1, static_cast<int64_t>(1234567));
    obj->put("test", arr);
    obj->erase("foo");
    if (!test_equal("simple", obj->str(),
                    "{\"value\":12,\"test\":[42,1234567]}",
                    "{\"test\":[42,1234567],\"value\":12}"))
        return false;
    return true;
}

bool test_quote() {
    auto obj = JsonObject::create();
    obj->put("foo", "");
    if (!test_equal("quote", obj->str(), "{\"foo\":\"\"}"))
        return false;
    obj->put("foo", "\"bar\"");
    if (!test_equal("quote", obj->str(), "{\"foo\":\"\\\"bar\\\"\"}"))
        return false;
    obj->put("foo", " ");
    if (!test_equal("quote", obj->str(), "{\"foo\":\" \"}"))
        return false;
    obj->put("foo", "\\\"");
    if (!test_equal("quote", obj->str(), "{\"foo\":\"\\\\\\\"\"}"))
        return false;
    obj->put("foo", "\n");
    if (!test_equal("quote", obj->str(), "{\"foo\":\"\\n\"}"))
        return false;
    return true;
}

}  // namespace

int main() {
    unsigned int ok = 0, tot = 0;

    tot++; if (test_simple()) ok++;
    tot++; if (test_quote()) ok++;

    std::cout << "OK " << ok << "/" << tot << std::endl;
    return ok == tot ? EXIT_SUCCESS : EXIT_FAILURE;
}
