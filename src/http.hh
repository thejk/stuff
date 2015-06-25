#ifndef HTTP_HH
#define HTTP_HH

#include <map>
#include <string>

namespace stuff {

class Http {
public:
    static void response(unsigned int status, const std::string& content);
    static void response(unsigned int status,
                         const std::map<std::string,std::string>& headers,
                         const std::string& content);

private:
    Http() = delete;
    ~Http() = delete;
};

}  // namespace stuff

#endif /* HTTP_HH */
