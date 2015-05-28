#include "common.hh"

#include "header_parser.hh"

namespace stuff {

namespace {

bool read_token(std::string::const_iterator* begin,
                const std::string::const_iterator& end,
                std::string* out) {
    auto i = *begin;
    if (i == end) return false;
    auto last = i;
    out->clear();
    do {
        if ((*i >= '^' && *i <= 'z') ||
            (*i >= '0' && *i <= '9') ||
            (*i >= '#' && *i <= '\'') ||
            *i == '!' || *i == '*' || *i == '+' || *i == '-' || *i == '.' ||
            *i == '|' || *i == '~') {
            ++i;
        } else if (*i >= 'A' && *i <= 'Z') {
            out->insert(out->end(), last, i);
            out->push_back('a' + *i - 'A');
            last = ++i;
        } else {
            break;
        }
    } while (i != end);
    out->insert(out->end(), last, i);
    *begin = i;
    return true;
}

bool read_quoted(std::string::const_iterator* begin,
                 const std::string::const_iterator& end,
                 std::string* out) {
    auto i = *begin;
    if (i == end || *i != '"') return false;
    auto last = ++i;
    out->clear();
    while (true) {
        if (*i == '"') {
            out->insert(out->end(), last, i);
            *begin = ++i;
            return true;
        } else if (*i == '\\') {
            out->insert(out->end(), last, i);
            if (++i == end) return false;
            if ((*i >= ' ' && *i <= '~') ||
                *i == '\t' ||
                (*i & 0x80)) {
                out->push_back(*i);
                last = ++i;
            } else {
                return false;
            }
        } else if ((*i >= '^' && *i <= '~') ||
                   (*i >= '#' && *i <= '[') ||
                   *i == '\t' || *i == ' ' ||
                   (*i & 0x80)) {
            ++i;
        } else {
            return false;
        }
    }
}

}  // namespace

bool HeaderParser::parse(const std::string& in, std::string* token,
                         std::map<std::string, std::string>* parameters) {
    auto i = in.begin();
    while (i != in.end() && (*i == ' ' || *i == '\t')) ++i;
    if (!read_token(&i, in.end(), token)) return false;
    while (true) {
        if (i == in.end() || *i != '/') break;
        std::string tmp;
        token->push_back('/');
        ++i;
        if (!read_token(&i, in.end(), &tmp)) return false;
        token->append(tmp);
    }
    if (parameters) parameters->clear();
    while (i != in.end()) {
        while (i != in.end() && (*i == ' ' || *i == '\t')) ++i;
        if (i == in.end()) break;
        if (*i != ';') return false;
        ++i;
        while (i != in.end() && (*i == ' ' || *i == '\t')) ++i;
        if (i == in.end()) return false;
        std::string key, value;
        if (!read_token(&i, in.end(), &key)) return false;
        if (i == in.end() || *i != '=') return false;
        ++i;
        if (i != in.end() && *i == '"') {
            if (!read_quoted(&i, in.end(), &value)) return false;
        } else {
            if (!read_token(&i, in.end(), &value)) return false;
        }
        if (parameters) (*parameters)[key] = value;
    }
    return true;
}

}  // namespace stuff
