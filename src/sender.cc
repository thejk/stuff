#include "common.hh"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <fcntl.h>
#include <iostream>
#include <netdb.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#include "config.hh"
#include "json.hh"

/*
{
    "username": "new-bot-name",

    "text": "<https://alert-system.com/alerts/1234|Click here> for details!"

    "icon_url": "https://slack.com/img/icons/app-57.png",
    "icon_emoji": ":ghost:"

    "channel": "#other-channel",    // A public channel override
    "channel": "@username",         // A Direct Message override
}
 */

using namespace stuff;

namespace {

void queue_message(const std::string& channel, const std::string& message);

class Client {
public:
    Client(int sock)
        : sock_(sock), fill_(0), have_channel_(false), size_(0) {
    }

    ~Client() {
        if (sock_ != -1) {
            close(sock_);
        }
    }

    int sock() const {
        return sock_;
    }

    bool read() {
        while (true) {
            char buf[1024];
            auto ret = ::read(sock_, buf, sizeof(buf));
            if (ret < 0) {
                if (errno == EINTR) continue;
                return errno == EWOULDBLOCK || errno == EAGAIN;
            }
            if (ret == 0) {
                return false;
            }
            size_t pos = 0;
            auto const fill = static_cast<size_t>(ret);
            while (pos < fill) {
                if (!have_channel_) {
                    if (size_ == 0) {
                        auto const avail = std::min(fill - pos,
                                                    static_cast<size_t>(4));
                        memcpy(buf_ + fill_, buf + pos, avail);
                        fill_ += avail;
                        pos += avail;
                        if (fill_ == 4) {
                            fill_ = 0;
                            memcpy(&size_, buf_, 4);
                            size_ = ntohl(size_);
                            if (size_ == 0) {
                                have_channel_ = true;
                            }
                        }
                    } else {
                        auto const avail = std::min(fill - pos,
                                                    size_ - channel_.size());
                        channel_.append(buf + pos, buf + pos + avail);
                        pos += avail;
                        if (channel_.size() == size_) {
                            have_channel_ = true;
                            size_ = 0;
                        }
                    }
                } else {
                    if (size_ == 0) {
                        auto const avail = std::min(fill - pos,
                                                    static_cast<size_t>(4));
                        memcpy(buf_ + fill_, buf + pos, avail);
                        fill_ += avail;
                        pos += avail;
                        if (fill_ == 4) {
                            fill_ = 0;
                            memcpy(&size_, buf_, 4);
                            size_ = ntohl(size_);
                            if (size_ == 0) {
                                send_message();
                            }
                        }
                    } else {
                        auto const avail = std::min(fill - pos,
                                                    size_ - message_.size());
                        message_.append(buf + pos, buf + pos + avail);
                        pos += avail;
                        if (message_.size() == size_) {
                            send_message();
                        }
                    }
                }
            }
        }
    }

private:
    void send_message() {
        assert(have_channel_);
        queue_message(channel_, message_);
        have_channel_ = false;
        size_ = 0;
        channel_.clear();
        message_.clear();
    }

    int sock_;

    char buf_[4];
    size_t fill_;

    bool have_channel_;
    uint32_t size_;
    std::string channel_;
    std::string message_;
};

bool g_quit;

void quit(int sig) {
    g_quit = true;
}

static size_t const MAX_CLIENTS = 64;

struct Info {
    std::string username;
    std::string icon_url;
    std::string icon_emoji;
};

Info g_info;

void queue_message(const std::string& channel, const std::string& message) {
    auto obj = JsonObject::create();
    obj->put("username", g_info.username);
    if (!g_info.icon_url.empty())
        obj->put("icon_url", g_info.icon_url);
    if (!g_info.icon_emoji.empty())
        obj->put("icon_emoji", g_info.icon_emoji);
    if (channel.empty()) return;
    obj->put("channel", "#" + channel);
    obj->put("text", message);

    
}

}  // namespace

int main() {
    auto cfg = Config::create();
    if (!cfg->load("./sender.config")) {
        cfg->load(SYSCONFDIR "/sender.config");
    }
    g_info.username = cfg->get("username", "stuff-sender-bot");
    g_info.icon_url = cfg->get("icon_url", "");
    g_info.icon_emoji = cfg->get("icon_emoji", "");
    auto const& listener = cfg->get("listener", "");
    if (listener.empty()) {
        std::cerr << "No listener configured" << std::endl;
        return EXIT_FAILURE;
    }

    int sock_ = -1;
    size_t pos = listener.find(':');
    if (pos != std::string::npos) {
        // [host]:port
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = PF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_PASSIVE;
        if (getaddrinfo(pos == 0 ? nullptr : listener.substr(0, pos).c_str(),
                        listener.substr(pos + 1).c_str(), &hints, &res)) {
            std::cerr << "Invalid host or port in: " << listener << std::endl;
            return EXIT_FAILURE;
        }
        for (auto ptr = res; ptr; ptr = ptr->ai_next) {
            sock_ = socket(ptr->ai_family, ptr->ai_socktype,
                           ptr->ai_protocol);
            if (sock_ == -1) continue;
            if (bind(sock_, res->ai_addr, res->ai_addrlen)) {
                close(sock_);
                sock_ = -1;
                continue;
            }
            break;
        }
        freeaddrinfo(res);
        if (sock_ == -1) {
            std::cerr << "Unable to bind: " << listener << std::endl;
            return EXIT_FAILURE;
        }
    } else {
        // socket
        sock_ = socket(PF_LOCAL, SOCK_STREAM, 0);
        if (sock_ == -1) {
            std::cerr << "Unable to create local socket: " << strerror(errno)
                      << std::endl;
            return EXIT_FAILURE;
        }
        struct sockaddr_un name;
        name.sun_family = AF_LOCAL;
        strncpy(name.sun_path, listener.c_str(), sizeof(name.sun_path));
        name.sun_path[sizeof(name.sun_path) - 1] = '\0';
        if (bind(sock_, reinterpret_cast<struct sockaddr*>(&name),
                 SUN_LEN(&name))) {
            std::cerr << "Bind failed: " << strerror(errno) << std::endl;
            close(sock_);
            sock_ = 1;
            return EXIT_FAILURE;
        }
    }

    if (listen(sock_, 10)) {
        std::cerr << "Listen failed: " << strerror(errno) << std::endl;
        close(sock_);
        return EXIT_FAILURE;
    }

    int value = 1;
    setsockopt(sock_, SOL_SOCKET, SO_REUSEPORT, &value, sizeof(value));

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, quit);
    signal(SIGTERM, quit);

    std::vector<Client> clients;

    while (!g_quit) {
        fd_set read_set;
        auto max = sock_ + 1;
        FD_ZERO(&read_set);
        FD_SET(sock_, &read_set);
        for (auto it = clients.begin(); it != clients.end();) {
            if (it->sock() == -1) {
                it = clients.erase(it);
            } else {
                max = std::max(max, it->sock() + 1);
                FD_SET(it->sock(), &read_set);
                ++it;
            }
        }
        auto ret = select(max, &read_set, nullptr, nullptr, nullptr);
        if (ret < 0) {
            if (errno == EINTR) continue;
            std::cerr << "Select failed: " << strerror(errno);
            close(sock_);
            return EXIT_FAILURE;
        }
        for (auto it = clients.begin(); ret > 0 && it != clients.end();) {
            if (FD_ISSET(it->sock(), &read_set)) {
                ret--;
                if (!it->read()) {
                    it = clients.erase(it);
                } else {
                    ++it;
                }
            } else {
                ++it;
            }
        }
        if (ret > 0 && FD_ISSET(sock_, &read_set)) {
            ret--;
            auto sock = accept(sock_, nullptr, nullptr);
            if (sock < 0) {
                if (errno == EINTR) continue;
                if (errno == EWOULDBLOCK || errno == EAGAIN) continue;
                std::cerr << "Accept failed: " << strerror(errno);
                close(sock_);
                return EXIT_FAILURE;
            }
            if (clients.size() == MAX_CLIENTS) {
                // Remove oldest
                clients.erase(clients.begin());
            }
            clients.emplace_back(sock);
        }
    }
    close(sock_);
    return EXIT_SUCCESS;
}
