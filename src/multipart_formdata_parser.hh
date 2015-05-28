#ifndef MULTIPART_FORMDATA_PARSER_HH
#define MULTIPART_FORMDATA_PARSER_HH

#include <map>
#include <string>
#include <vector>

namespace stuff {

class MultipartFormDataParser {
public:
    static bool parse(const std::vector<char>& in,
                      const std::string& boundary,
                      std::map<std::string, std::string>* out);

private:
    MultipartFormDataParser() {}
    ~MultipartFormDataParser() {}
    MultipartFormDataParser(const MultipartFormDataParser&) = delete;
    MultipartFormDataParser& operator=(const MultipartFormDataParser&) = delete;
};

}  // namespace stuff

#endif /* MULTIPART_FORMDATA_PARSER_HH */
