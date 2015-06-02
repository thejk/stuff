#include "common.hh"

#include <cstdlib>
#if HAVE_FASTCGI
#include <fcgio.h>
#endif
#include <memory>

#include "cgi.hh"
#include "header_parser.hh"
#include "multipart_formdata_parser.hh"
#include "strutils.hh"
#include "query_parser.hh"

namespace stuff {

namespace {

class CGIImpl : public CGI {
public:
    void post_data(std::vector<char>* data) override {
        if (!have_post_data_) {
            fill_post_data();
        }
        data->assign(post_data_.begin(), post_data_.end());
    }

    bool post_data(std::map<std::string, std::string>* data) override {
        data->clear();
        if (!have_post_data_) {
            fill_post_data();
        }

        auto ct = getparam("CONTENT_TYPE");
        if (!ct) return false;

        std::string type;
        std::map<std::string, std::string> params;
        if (!HeaderParser::parse(ct, &type, &params)) {
            return false;
        }
        if (type.compare("application/x-www-form-urlencoded") == 0) {
            return QueryParser::parse(std::string(post_data_.data(),
                                                  post_data_.size()), data);
        } else if (type.compare("multipart/form-data") == 0) {
            auto it = params.find("boundary");
            if (it == params.end()) return false;
            return MultipartFormDataParser::parse(post_data_, it->second, data);
        }
        return false;
    }

    void query_data(std::map<std::string, std::string>* data) override {
        data->clear();
        auto qs = getparam("QUERY_STRING");
        if (qs) {
            QueryParser::parse(qs, data);
        }
    }

    std::string request_path() override {
        auto path = getparam("PATH_INFO");
        return path ? path : "";
    }

    std::string content_type() override {
        auto ct = getparam("CONTENT_TYPE");
        if (!ct) return "";
        std::string type;
        if (!HeaderParser::parse(ct, &type, nullptr)) return "";
        return type;
    }

    enum request_type request_type() override {
        auto p = getparam("REQUEST_METHOD");
        if (p) {
            auto method = ascii_tolower(p);
            if (method.compare("get") == 0) return GET;
            if (method.compare("post") == 0) return POST;
            if (method.compare("head") == 0) return HEAD;
            if (method.compare("put") == 0) return PUT;
            if (method.compare("trace") == 0) return TRACE;
        }
        return UNKNOWN;
    }

    virtual void reset() {
        have_post_data_ = false;
        post_data_.clear();
    }

protected:
    CGIImpl()
        : have_post_data_(false) {
    }

    virtual const char* getparam(const char* name) = 0;

private:
    void fill_post_data() {
        have_post_data_ = true;
        switch (request_type()) {
        case POST:
        case PUT: {
            auto len = getparam("CONTENT_LENGTH");
            std::string tmpstr;
            if (len && HeaderParser::parse(len, &tmpstr, nullptr)) {
                char* end = nullptr;
                errno = 0;
                auto tmp = strtoul(tmpstr.c_str(), &end, 10);
                if (errno == 0 && end && !*end) {
                    post_data_.resize(tmp);
                    std::cin.read(post_data_.data(), tmp);
                    post_data_.resize(std::cin.gcount());
                    return;
                }
            }
            while (std::cin.good()) {
                char buf[1024];
                std::cin.read(buf, sizeof(buf));
                post_data_.insert(post_data_.end(),
                                  buf, buf + std::cin.gcount());
            }
            break;
        }
        case GET:
        case HEAD:
        case TRACE:
        case UNKNOWN:
            break;
        }
    }

    bool have_post_data_;
    std::vector<char> post_data_;
};

class BasicCGIImpl : public CGIImpl {
public:
    BasicCGIImpl()
        : CGIImpl() {
    }
    ~BasicCGIImpl() override {
    }
protected:
    const char* getparam(const char* name) override {
        return getenv(name);
    }
};

#if HAVE_FASTCGI
class FastCGIImpl : public CGIImpl {
public:
    FastCGIImpl()
        : CGIImpl(), params_(nullptr) {
    }
    ~FastCGIImpl() override {
    }

    void reset(FCGX_ParamArray params) {
        CGIImpl::reset();
        params_ = params;
    }
protected:
    using CGIImpl::reset;
    const char* getparam(const char* name) override {
        return params_ ? FCGX_GetParam(name, params_) : nullptr;
    }
private:
    FCGX_ParamArray params_;
};
#endif

}  // namespace

void CGI::get_data(std::map<std::string,std::string>* data) {
    if (post_data(data)) return;
    query_data(data);
}


int CGI::run(std::function<bool(CGI*)> handle_request) {
#if HAVE_FASTCGI
    if (!FCGX_IsCGI()) {
        std::unique_ptr<FastCGIImpl> cgi(new FastCGIImpl());
        auto cin_streambuf = std::cin.rdbuf();
        auto cout_streambuf = std::cout.rdbuf();
        auto cerr_streambuf = std::cerr.rdbuf();

        FCGX_Stream* in;
        FCGX_Stream* out;
        FCGX_Stream* err;
        FCGX_ParamArray params;
        while (FCGX_Accept(&in, &out, &err, &params) == 0) {
            cgi->reset(params);
            fcgi_streambuf cin_fcgi_streambuf(in);
            fcgi_streambuf cout_fcgi_streambuf(out);
            fcgi_streambuf cerr_fcgi_streambuf(err);

            std::cin.rdbuf(&cin_fcgi_streambuf);
            std::cout.rdbuf(&cout_fcgi_streambuf);
            std::cerr.rdbuf(&cerr_fcgi_streambuf);

            if (!handle_request(cgi.get())) {
                FCGX_SetExitStatus(-1, out);
                FCGX_Finish();
                return -1;
            }
        }

        std::cin.rdbuf(cin_streambuf);
        std::cout.rdbuf(cout_streambuf);
        std::cerr.rdbuf(cerr_streambuf);

        return 0;
    }
#endif
    std::unique_ptr<CGIImpl> cgi(new BasicCGIImpl());
    return handle_request(cgi.get()) ? 0 : -1;
}

}  // namespace stuff

