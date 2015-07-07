#include "common.hh"

#include <algorithm>

#include "header_parser.hh"
#include "multipart_formdata_parser.hh"
#include "strutils.hh"

namespace stuff {

namespace {

template<typename Iterator>
Iterator find_boundary(Iterator begin, Iterator end,
                       const std::string& boundary,
                       bool* last) {
    Iterator start, test;
    for (auto it = begin; it != end; ++it) {
        if (it == begin && *it == '-') {
            start = it;
            test = it + 1;
            if (test == end || *test != '-') continue;
        } else if (*it == '\r') {
            start = it;
            test = it + 1;
            if (test == end || *test != '\n') continue;
            ++test;
            if (test == end || *test != '-') continue;
            ++test;
            if (test == end || *test != '-') continue;
        } else {
            continue;
        }
        ++test;
        if (static_cast<size_t>(end - test) <= boundary.size()) break;
        if (boundary.compare(0, std::string::npos,
                             &(*test), boundary.size()) == 0) {
            test += boundary.size();
            if (test == end) break;
            if (*test == '-') {
                ++test;
                if (test == end || *test != '-') continue;
                *last = true;
                return start;
            } else if (*test == '\r') {
                ++test;
                if (test == end || *test != '\n') continue;
                *last = false;
                return start;
            }
        }
    }
    *last = true;
    return end;
}

template<typename Iterator>
bool parse_part(Iterator begin, Iterator end,
                std::map<std::string, std::string>* out) {
    static const char EOL[] = "\r\n";
    bool have_name = false, ok_contenttype = true, ok_encoding = true,
        ok_content = true;
    std::string name;
    while (true) {
        auto eol = std::search(begin, end, EOL, EOL + 2);
        if (eol == end) return false;
        if (eol == begin) {
            begin += 2;
            break;
        }
        auto colon = std::find(begin, eol, ':');
        if (colon == eol) return false;
        std::string header =
            ascii_tolower(std::string(&(*begin), colon - begin));
        ++colon;
        if (colon == eol) return false;
        if (header == "content-disposition" || header == "content-type" ||
            header == "content-transfer-encoding") {
            std::string token;
            std::map<std::string, std::string> params;
            std::string value = std::string(&(*colon), eol - colon);
            if (!HeaderParser::parse(std::string(&(*colon), eol - colon),
                                     &token, &params)) return false;
            if (header[9] == 'i') {  // content-disposition
                if (token == "form-data") {
                    auto it = params.find("name");
                    if (it == params.end()) return false;
                    have_name = true;
                    name = it->second;
                    ok_content = true;
                } else {
                    ok_content = false;
                }
            } else if (header[9] == 'y') {  // content-type
                auto pos = token.find('/');
                if (pos == std::string::npos) return false;
                ok_contenttype = true;
                if (token.compare(0, pos, "text") != 0) ok_contenttype = false;
                if (ok_contenttype) {
                    auto it = params.find("charset");
                    if (it != params.end()) {
                        std::string charset = ascii_tolower(it->second);
                        ok_contenttype = charset == "ascii" ||
                            charset == "us-ascii" ||
                            charset == "utf-8";
                    }
                }
            } else /* if (header[9] == 'r') */ {  // content-transfer-encoding
                ok_encoding = token == "7bit" || token == "8bit" ||
                    token == "binary" || token == "identity";
            }
        }
        begin = eol + 2;
    }
    if (have_name && ok_contenttype && ok_encoding && ok_content) {
        (*out)[name] = std::string(&(*begin), end - begin);
    }
    return true;
}

}  // namespace

bool MultipartFormDataParser::parse(const std::vector<char>& in,
                                    const std::string& boundary,
                                    std::map<std::string, std::string>* out) {
    bool last;
    auto start = find_boundary(in.begin(), in.end(), boundary, &last);
    out->clear();
    if (start == in.end()) return in.empty();
    if (last) return true;
    if (*start != '-') start += 2;
    while (true)
    {
        start += boundary.size() + 4;
        auto end = find_boundary(start, in.end(), boundary, &last);
        if (end == in.end()) return false;

        if (!parse_part(start, end, out)) {
            return false;
        }

        if (last) return true;

        start = end + 2;
    }
}

}  // namespace stuff
