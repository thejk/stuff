#ifndef CGI_HH
#define CGI_HH

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace stuff {

class CGI {
public:
    enum request_type {
        GET,
        HEAD,
        POST,
        PUT,
        TRACE,

        UNKNOWN,
    };

    virtual void post_data(std::vector<char>* data) = 0;
    // Return true if post data is multipart, false otherwise
    virtual bool post_data(std::map<std::string,std::string>* data) = 0;
    virtual void query_data(std::map<std::string,std::string>* data) = 0;
    // Will first try post_data and fallback to query_data
    void get_data(std::map<std::string,std::string>* data);
    virtual std::string request_path() = 0;
    virtual std::string request_uri() = 0;
    virtual std::string remote_addr() = 0;
    virtual request_type request_type() = 0;
    virtual std::string content_type() = 0;
    virtual std::string http_auth() = 0;

    static int run(std::function<bool(CGI*)> handle_request);

protected:
    CGI() {}
    virtual ~CGI() {}
    CGI(const CGI&) = delete;
    CGI& operator=(const CGI&) = delete;
};

}  // namespace stuff

#endif /* CGI_HH */
