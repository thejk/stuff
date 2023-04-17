#include "common.hh"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/wait.h>
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

long WRITE_TIMEOUT = 5;
long CONNECT_TIMEOUT = 5;

class SenderClientImpl : public SenderClient {
public:
    SenderClientImpl(std::shared_ptr<Error> error)
        : error_(error) {
    }

    bool open(const Config* config) {
        if (!config) return false;
        sender_ = config->get("sender", "");
        if (sender_.empty()) {
            if (error_) error_->error("Config missing sender");
            return false;
        }
        sender_bin_ = config->get("sender_bin", "");
        return true;
    }

    void send(const std::string& channel, const std::string& message) override {
        struct timeval target;
        gettimeofday(&target, NULL);
        target.tv_sec += WRITE_TIMEOUT;

        send(channel, message, &target);
    }

private:
    void send(const std::string& channel, const std::string& message,
              const struct timeval* target) {
        if (!sock_) {
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
                ret = write(sock_.get(),
                            reinterpret_cast<char*>(&size1) + pos, avail);
                if (ret > 0) {
                    pos += ret;
                    if (static_cast<size_t>(ret) == avail) continue;
                }
            } else if (pos < 4 + channel.size()) {
                size_t const avail = 4 + channel.size() - pos;
                ret = write(sock_.get(), channel.data() + pos - 4, avail);
                if (ret > 0) {
                    pos += ret;
                    if (static_cast<size_t>(ret) == avail) continue;
                }
            } else if (pos < 8 + channel.size()) {
                size_t const avail = 8 + channel.size() - pos;
                ret = write(sock_.get(), reinterpret_cast<char*>(&size2)
                            + pos - 4 - channel.size(), avail);
                if (ret > 0) {
                    pos += ret;
                    if (static_cast<size_t>(ret) == avail) continue;
                }
            } else {
                size_t const avail = len - pos;
                ret = write(sock_.get(),
                            message.data() + pos - 8 - channel.size(),
                            avail);
                if (ret > 0) {
                    pos += ret;
                    if (static_cast<size_t>(ret) == avail) continue;
                }
            }

            if (ret < 0) {
                if (errno == EINTR) continue;
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    sock_.reset();
                    return send(channel, message);
                }
            }

            fd_set write_set;
            FD_ZERO(&write_set);
            FD_SET(sock_.get(), &write_set);
            while (true) {
                struct timeval timeout;
                if (!calc_timeout(target, &timeout)) {
                    timeout.tv_sec = 0;
                    timeout.tv_usec = 0;
                }
                auto ret = select(sock_.get() + 1, nullptr, &write_set, nullptr,
                                  &timeout);
                if (ret < 0 && errno == EINTR) continue;
                if (ret <= 0) {
                    // Timeout or error
                    sock_.reset();
                    return send(channel, message);
                }
                break;
            }
        }
    }

    bool connect_timeout(int sock, struct sockaddr* addr, socklen_t addrlen,
                         const struct timeval* target) {
        if (!make_nonblocking(sock)) {
            error_->error("Unable to make non-blocking socket", errno);
            return false;
        }
        while (true) {
            if (connect(sock, addr, addrlen) == 0) {
                return true;
            }
            if (errno == EINTR) continue;
            if (errno != EINPROGRESS) return false;
            fd_set write_set;
            FD_ZERO(&write_set);
            FD_SET(sock, &write_set);
            while (true) {
                struct timeval timeout;
                if (!calc_timeout(target, &timeout)) {
                    timeout.tv_sec = 0;
                    timeout.tv_usec = 0;
                }
                auto ret = select(sock + 1, nullptr, &write_set, nullptr,
                                  &timeout);
                if (ret < 0) {
                    if (errno == EINTR) continue;
                    return false;
                }
                if (ret == 0) {
                    errno = ETIMEDOUT;
                    return false;
                }
                int err;
                socklen_t len = sizeof(int);
                if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len)) {
                    return false;
                }
                if (err != 0) {
                    errno = err;
                    return false;
                }
                return true;
            }
        }
    }

    bool setup_start() {
        if (sender_bin_.empty()) return false;
        auto pid = fork();
        if (pid < 0) {
            if (error_) error_->error("Error forking", errno);
            return false;
        }
        if (pid == 0) {
            char* argv[2];
            argv[0] = const_cast<char*>(sender_bin_.c_str());
            argv[1] = nullptr;
            _exit(execv(argv[0], argv));
        } else {
            int status;
            auto ret = waitpid(pid, &status, 0);
            if (ret == -1) {
                if (error_) {
                    error_->error("Error waiting for sender bin", errno);
                }
            }
            return true;
        }
    }

    bool setup() {
        struct timeval target;
        gettimeofday(&target, NULL);
        target.tv_sec += CONNECT_TIMEOUT;

        auto pos = sender_.find(':');
        if (pos != std::string::npos) {
            // host:port
            struct addrinfo hints, *res;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = PF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;
            if (getaddrinfo(sender_.substr(0, pos).c_str(),
                            sender_.substr(pos + 1).c_str(), &hints, &res)) {
                if (error_) error_->error("Error resolving: " + sender_);
                return false;
            }
            for (auto ptr = res; ptr; ptr = ptr->ai_next) {
                sock_.reset(socket(ptr->ai_family, ptr->ai_socktype,
                                   ptr->ai_protocol));
                if (!sock_) continue;
                if (!connect_timeout(sock_.get(), res->ai_addr, res->ai_addrlen,
                                     &target)) {
                    sock_.reset();
                    continue;
                }
                break;
            }
            freeaddrinfo(res);
            if (!sock_) {
                if (errno == ECONNREFUSED && setup_start()) {
                    return setup();
                }
                if (error_) error_->error("Socket/Connect failed", errno);
                return false;
            }
        } else {
            // socket
            sock_.reset(socket(PF_LOCAL, SOCK_STREAM, 0));
            if (!sock_) {
                if (error_) {
                    error_->error("Unable to create unix socket", errno);
                }
                return false;
            }
            struct sockaddr_un name;
            name.sun_family = AF_LOCAL;
            strncpy(name.sun_path, sender_.c_str(), sizeof(name.sun_path));
            name.sun_path[sizeof(name.sun_path) - 1] = '\0';
            if (!connect_timeout(sock_.get(),
                                 reinterpret_cast<struct sockaddr*>(&name),
                                 SUN_LEN(&name), &target)) {
                if ((errno == ECONNREFUSED ||
                     errno == ENOENT) && setup_start()) {
                    return setup();
                }
                if (error_) error_->error("Connect failed", errno);
                sock_.reset();
                return false;
            }
        }

        return true;
    }

    std::string sender_;
    std::string sender_bin_;
    std::shared_ptr<Error> error_;
    sockguard sock_;
};

}  // namespace

// static
std::unique_ptr<SenderClient> SenderClient::create(
        const Config* config, std::shared_ptr<Error> error) {
    std::unique_ptr<SenderClientImpl> ret(new SenderClientImpl(error));
    if (!ret->open(config)) return nullptr;
    return ret;
}

}  // namespace stuff
