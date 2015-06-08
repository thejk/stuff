#include "common.hh"

#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>

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

bool calc_timeout(const struct timeval* target, struct timeval* timeout) {
    gettimeofday(timeout, nullptr);
    if (target->tv_sec == timeout->tv_sec) {
        timeout->tv_sec = 0;
        if (target->tv_usec >= timeout->tv_usec) {
            timeout->tv_usec = target->tv_usec - timeout->tv_usec;
        } else {
            return false;
        }
    } else if (target->tv_sec > timeout->tv_sec) {
        timeout->tv_sec = target->tv_sec - timeout->tv_sec;
        if (target->tv_usec >= timeout->tv_usec) {
            timeout->tv_usec = target->tv_usec - timeout->tv_usec;
        } else {
            timeout->tv_sec--;
            timeout->tv_usec = 1000000l + target->tv_usec - timeout->tv_usec;
        }
    } else {
        return false;
    }
    return true;
}

// static
void sockguard::close(int sock) {
    ::close(sock);
}

}  // namespace stuff

namespace std {
void swap(stuff::sockguard& s1, stuff::sockguard& s2) noexcept {
    s1.swap(s2);
}
}  // namespace std
