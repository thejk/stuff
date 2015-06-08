#include "common.hh"

#include <fcntl.h>

#include "sockutils.hh"

namespace stuff {

bool make_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    if (!(flags & O_NONBLOCK)) {
        flags |= O_NONBLOCK;
        if (fcntl(sock, F_SETFL, flags) < 0) {
            return false;
        }
    }
    return true;
}

}  // namespace stuff
