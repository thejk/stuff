#ifndef HTTP_HH
#define HTTP_HH

#include <string>

namespace stuff {

class Http {
public:
    static void response(unsigned int status, const std::string& content);

private:
    Http() = delete;
    ~Http() = delete;
};

}  // namespace stuff

#endif /* HTTP_HH */
