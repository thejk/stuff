#include "common.hh"

#include <cstdarg>
#include <cstdlib>
#include <iostream>

#include "multipart_formdata_parser.hh"

using namespace stuff;

namespace {

bool test(const std::string& in, const char* boundary, ...) {
    std::map<std::string, std::string> tmp;
    std::vector<char> data(in.begin(), in.end());
    if (!MultipartFormDataParser::parse(data, boundary, &tmp)) {
        std::cerr << "expected success: " << in << std::endl;
        return false;
    }
    va_list args;
    va_start(args, boundary);
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

bool test_fail(const std::string& in, const std::string& boundary) {
    std::map<std::string, std::string> tmp;
    std::vector<char> data(in.begin(), in.end());
    if (!MultipartFormDataParser::parse(data, boundary, &tmp)) return true;
    std::cerr << "expected fail: " << in << std::endl;
    return false;
}

}  // namespace

int main() {
    unsigned int ok = 0, tot = 0;

    tot++; if (test("", "AaB03x", NULL)) ok++;

    tot++; if (test(
"--AaB03x\r\n"
"Content-Disposition: form-data; name=\"submit-name\"\r\n"
"\r\n"
"Larry\r\n"
"--AaB03x\r\n"
"Content-Disposition: form-data; name=\"files\"; filename=\"file1.txt\"\r\n"
"Content-Type: text/plain\r\n"
"\r\n"
"... contents of file1.txt ...\r\n"
"--AaB03x--\r\n",
"AaB03x",
"submit-name", "Larry",
"files", "... contents of file1.txt ...",
NULL)) ok++;

    tot++; if (test(
"--AaB03x\r\n"
"Content-Disposition: form-data; name=\"submit-name\"\r\n"
"\r\n"
"Larry\r\n"
"--AaB03x\r\n"
"Content-Disposition: form-data; name=\"files\"\r\n"
"Content-Type: multipart/mixed; boundary=BbC04y\r\n"
"\r\n"
"--BbC04y\r\n"
"Content-Disposition: file; filename=\"file1.txt\"\r\n"
"Content-Type: text/plain\r\n"
"\r\n"
"... contents of file1.txt ...\r\n"
"--BbC04y\r\n"
"Content-Disposition: file; filename=\"file2.gif\"\r\n"
"Content-Type: image/gif\r\n"
"Content-Transfer-Encoding: binary\r\n"
"\r\n"
"...contents of file2.gif...\r\n"
"--BbC04y--\r\n"
"--AaB03x--\r\n",
"AaB03x",
"submit-name", "Larry",
NULL)) ok++;

    tot++; if (test_fail("--X", "X")) ok++;

    std::cout << "OK " << ok << "/" << tot << std::endl;
    return ok == tot ? EXIT_SUCCESS : EXIT_FAILURE;
}
