#ifndef HEADER_PARSER_HH
#define HEADER_PARSER_HH

#include <map>
#include <string>

namespace stuff {

class HeaderParser {
public:
    // token and keys to parameters will all be returned as lowercase ascii
    static bool parse(const std::string& in, std::string* token,
                      std::map<std::string, std::string>* parameters);

private:
    HeaderParser() {}
    ~HeaderParser() {}
    HeaderParser(const HeaderParser&) = delete;
    HeaderParser& operator=(const HeaderParser&) = delete;
};

}  // namespace stuff

#endif /* HEADER_PARSER_HH */
