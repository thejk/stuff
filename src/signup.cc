#include "common.hh"

#include "cgi.hh"
#include "db.hh"

using namespace stuff;

namespace {

bool handle_request(CGI* cgi) {
    
    return false;
}

}  // namespace

int main() {
    return CGI::run(handle_request);
}
