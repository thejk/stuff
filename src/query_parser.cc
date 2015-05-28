#include "common.hh"

#include "query_parser.hh"

namespace stuff {

namespace {

bool hex(char in, uint8_t* out) {
    if (in >= '0' && in <= '9') {
        *out = in - '0';
    } else if (in >= 'A' && in <= 'F') {
        *out = 10 + in - 'A';
    } else if (in >= 'a' && in <= 'f') {
        *out = 10 + in - 'a';
    } else {
        return false;
    }
    return true;
}

}  // namespace

bool QueryParser::parse(const std::string& in,
                        std::map<std::string, std::string>* out) {
    out->clear();
    if (in.empty()) return true;
    auto i = in.begin();
    if (*i == '?' || *i == '&') {
        ++i;
    }
    std::string key, value;
    bool have_key = false;
    while (true) {
        char c;
        if (i == in.end() || *i == '&') {
            if (!have_key && key.empty()) return false;
            (*out)[key] = value;
            if (i == in.end()) return true;
            have_key = false;
            key.clear();
            value.clear();
            if (++i == in.end()) return true;
            continue;
        } else if (*i == '=') {
            if (have_key) return false;
            if (key.empty()) return false;
            have_key = true;
            ++i;
            continue;
        } else if (*i == '+') {
            c = ' ';
        } else if (*i == '%') {
            if (++i == in.end()) return false;
            uint8_t h, l;
            if (!hex(*i, &h)) return false;
            if (++i == in.end()) return false;
            if (!hex(*i, &l)) return false;
            c = (h << 4) | l;
        } else if ((*i >= 'a' && *i <= 'z') ||
                   (*i >= '@' && *i <= 'Z') ||
                   (*i >= '0' && *i <= ';') ||
                   (*i >= '\'' && *i <= '*') ||
                   (*i >= ',' && *i <= '.') ||
                   *i == '!' || *i == '$' ||
                   *i == '_' || *i == '~') {
            c = *i;
        } else {
            // Character not allowed
            return false;
        }
        if (have_key) {
            value.push_back(c);
        } else {
            key.push_back(c);
        }
        ++i;
    }
}


}  // namespace stuff
