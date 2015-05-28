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

}  // namespace stuff
