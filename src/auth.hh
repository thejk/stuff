#ifndef AUTH_HH
#define AUTH_HH

#include <string>

namespace stuff {

class CGI;

class Auth {
public:
    static bool auth(CGI* cgi, const std::string& realm,
                     const std::string& passwd,
                     std::string* user);

private:
    Auth() = delete;
    ~Auth() = delete;
    Auth(Auth&) = delete;
    Auth& operator=(Auth&) = delete;
};

}  // namespace stuff

#endif /* AUTH_HH */
