#ifndef QUERY_PARSER_HH
#define QUERY_PARSER_HH

#include <map>
#include <string>

namespace stuff {

class QueryParser {
public:
    static bool parse(const std::string& in,
                      std::map<std::string, std::string>* out);

private:
    QueryParser() {}
    ~QueryParser() {}
    QueryParser(const QueryParser&) = delete;
    QueryParser& operator=(const QueryParser&) = delete;
};

}  // namespace stuff

#endif /* QUERY_PARSER_HH */
