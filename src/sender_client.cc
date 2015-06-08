#include "common.hh"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>

#include "config.hh"
#include "sender_client.hh"
#include "sockutils.hh"

namespace stuff {

namespace {

class SenderClientImpl : public SenderClient {
public:
    SenderClientImpl()
        : sock_(-1) {
    }
    ~SenderClientImpl() override {
        if (sock_ != -1) {
            close(sock_);
        }
    }

    bool open(const Config* config) {
        if (!config) return false;
        sender_ = config->get("sender", "");
        if (sender_.empty()) return false;
        return true;
    }

    void send(const std::string& channel, const std::string& message) override {
        struct timeval target;
        gettimeofday(&target, NULL);
        target.tv_sec += 5;

        send(channel, message, &target);
    }

private:
    void send(const std::string& channel, const std::string& message,
              const struct timeval* target) {
        if (sock_ == -1) {
            if (!setup()) return;
        }

        uint32_t size1 = channel.size();
        uint32_t size2 = message.size();
        size_t pos = 0, len = 8 + size1 + size2;
        size1 = htonl(size1);
        size2 = htonl(size2);
        while (pos < len) {
            ssize_t ret;
            if (pos < 4) {
                size_t const avail = 4 - pos;
                ret = write(sock_,
                            reinterpret_cast<char*>(&size1) + pos, avail);
                if (ret > 0) {
                    pos += ret;
                    if (static_cast<size_t>(ret) == avail) continue;
                }
            } else if (pos < 4 + channel.size()) {
                size_t const avail = 4 + channel.size() - pos;
                ret = write(sock_, channel.data() + pos - 4, avail);
                if (ret > 0) {
                    pos += ret;
                    if (static_cast<size_t>(ret) == avail) continue;
                }
            } else if (pos < 8 + channel.size()) {
                size_t const avail = 8 + channel.size() - pos;
                ret = write(sock_, reinterpret_cast<char*>(&size2)
                            + pos - 4 - channel.size(), avail);
                if (ret > 0) {
                    pos += ret;
                    if (static_cast<size_t>(ret) == avail) continue;
                }
            } else {
                size_t const avail = len - pos;
                ret = write(sock_, message.data() + pos - 8 - channel.size(),
                            avail);
                if (ret > 0) {
                    pos += ret;
                    if (static_cast<size_t>(ret) == avail) continue;
                }
            }

            if (ret < 0) {
                if (errno == EINTR) continue;
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    close(sock_);
                    sock_ = -1;
                    return send(channel, message);
                }
            }

            fd_set write_set;
            FD_ZERO(&write_set);
            FD_SET(sock_, &write_set);
            while (true) {
                struct timeval timeout;
                gettimeofday(&timeout, NULL);
                if (target->tv_sec == timeout.tv_sec) {
                    timeout.tv_sec = 0;
                    if (target->tv_usec > timeout.tv_usec) {
                        timeout.tv_usec = target->tv_usec - timeout.tv_usec;
                    } else {
                        timeout.tv_usec = 0;
                    }
                } else if (target->tv_sec > timeout.tv_sec) {
                    timeout.tv_sec = target->tv_sec - timeout.tv_sec;
                    if (target->tv_usec >= timeout.tv_usec) {
                        timeout.tv_usec = target->tv_usec - timeout.tv_usec;
                    } else {
                        timeout.tv_sec--;
                        timeout.tv_usec =
                            1000000l + target->tv_usec - timeout.tv_usec;
                    }
                } else {
                    timeout.tv_sec = 0;
                    timeout.tv_usec = 0;
                }
                auto ret = select(sock_ + 1, nullptr, &write_set, nullptr,
                                  &timeout);
                if (ret < 0 && errno == EINTR) continue;
                if (ret <= 0) {
                    // Timeout or error
                    close(sock_);
                    sock_ = -1;
                    return send(channel, message);
                }
                break;
            }
        }
    }

    bool setup() {
        size_t pos = sender_.find(':');
        if (pos != std::string::npos) {
            // host:port
            struct addrinfo hints, *res;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = PF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;
            if (getaddrinfo(sender_.substr(0, pos).c_str(),
                            sender_.substr(pos + 1).c_str(), &hints, &res)) {
                return false;
            }
            for (auto ptr = res; ptr; ptr = ptr->ai_next) {
                sock_ = socket(ptr->ai_family, ptr->ai_socktype,
                               ptr->ai_protocol);
                if (sock_ == -1) continue;
                // TODO: Make async
                if (connect(sock_, res->ai_addr, res->ai_addrlen)) {
                    close(sock_);
                    sock_ = -1;
                    continue;
                }
                break;
           }
           freeaddrinfo(res);
           if (sock_ == -1) return false;
        } else {
            // socket
            sock_ = socket(PF_LOCAL, SOCK_STREAM, 0);
            if (sock_ == -1) return false;
            struct sockaddr_un name;
            name.sun_family = AF_LOCAL;
            strncpy(name.sun_path, sender_.c_str(), sizeof(name.sun_path));
            name.sun_path[sizeof(name.sun_path) - 1] = '\0';
            // TODO: Make async
            if (connect(sock_, reinterpret_cast<struct sockaddr*>(&name),
                        SUN_LEN(&name))) {
                close(sock_);
                sock_ = 1;
                return false;
            }
        }

        if (!make_nonblocking(sock_)) {
            close(sock_);
            sock_ = -1;
            return false;
        }

        return true;
    }

    std::string sender_;
    int sock_;
};

}  // namespace

// static
std::unique_ptr<SenderClient> SenderClient::create(const Config* config) {
    std::unique_ptr<SenderClientImpl> ret(new SenderClientImpl());
    if (!ret->open(config)) return nullptr;
    return std::move(ret);
}

}  // namespace stuff
