#include "common.hh"

#include "auth.hh"
#include "base64.hh"
#include "cgi.hh"
#include "http.hh"
#include "strutils.hh"

namespace stuff {

bool Auth::auth(CGI* cgi, const std::string& realm, const std::string& passwd,
                std::string* user) {
    auto auth = cgi->http_auth();
    auto pos = auth.find(' ');
    if (pos != std::string::npos) {
        if (ascii_tolower(auth.substr(0, pos)) == "basic") {
            std::string tmp;
            if (Base64::decode(auth.substr(pos + 1), &tmp)) {
                pos = tmp.find(':');
                if (pos != std::string::npos) {
                    if (tmp.substr(pos + 1) == passwd) {
                        if (user) user->assign(tmp.substr(0, pos));
                        return true;
                    }
                }
            }
        }
    }

    std::map<std::string, std::string> headers;
    std::string tmp = realm;
    for (auto it = tmp.begin(); it != tmp.end(); ++it) {
        if (!((*it >= 'a' && *it <= 'z') ||
              (*it >= 'A' && *it <= 'Z') ||
              (*it >= '0' && *it <= '9') ||
              *it == '-' || *it == '_' || *it == '.' || *it == ' ')) {
            *it = '.';
        }
    }
    headers.insert(std::make_pair("WWW-Authenticate",
                                  "Basic realm=\"" + tmp + "\""));
    Http::response(401, headers, "Authentication needed");
    return false;
}

}  // namespace stuff
