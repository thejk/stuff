#include "common.hh"

#include "strutils.hh"

namespace stuff {

std::string ascii_tolower(const std::string& str) {
    for (auto it = str.begin(); it != str.end(); ++it) {
        if (*it >= 'A' && *it <= 'Z') {
            std::string ret(str.begin(), it);
            ret.push_back('a' + *it - 'A');
            auto last = ++it;
            while (it != str.end()) {
                if (*it >= 'A' && *it <= 'Z') {
                    ret.append(last, it);
                    ret.push_back('a' + *it - 'A');
                    last = ++it;
                } else {
                    ++it;
                }
            }
            ret.append(last, it);
            return ret;
        }
    }
    return str;
}

namespace {
bool is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}
}  // namespace

std::string trim(const std::string& str) {
    auto start = str.begin();
    while (start != str.end() && is_ws(*start)) start++;
    auto end = str.end() - 1;
    while (end >= start && is_ws(*end)) end--;
    return std::string(start, end + 1);
}

}  // namespace stuff
