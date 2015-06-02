#include "common.hh"

#include <iostream>

#include "http.hh"

namespace stuff {

namespace {

const std::string EOL = "\r\n";

const char* get_status_message(unsigned int status) {
    switch (status) {
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 203: return "Partial information";
    case 204: return "No response";
    case 301: return "Moved";
    case 302: return "Found";
    case 304: return "Not modified";
    case 400: return "Bad request";
    case 401: return "Unauthorized";
    case 402: return "Payment required";
    case 403: return "Forbidden";
    case 404: return "Not found";
    case 500: return "Internal error";
    case 501: return "Not implemented";
    }
    return nullptr;
}

}  // namespace

// static
void Http::response(unsigned int status, const std::string& content) {
    if (status != 200) {
        std::cout << "Status: " << status;
        const char* msg = get_status_message(status);
        if (msg) {
            std::cout << ' ' << msg;
        }
        std::cout << EOL;
    }
    std::cout << "Content-Type: text/plain; charset=utf-8" << EOL;
    std::cout << "Content-Length: " << content.size() << EOL;
    std::cout << EOL;
    std::cout << content;
}

}  // namespace stuff
