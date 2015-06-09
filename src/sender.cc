#include "common.hh"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <iostream>
#include <netdb.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#include <curl/curl.h>

#include "config.hh"
#include "json.hh"
#include "sockutils.hh"

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

    int sock() const {
        return sock_.get();
    }

    bool read() {
        while (true) {
            char buf[1024];
            auto ret = ::read(sock_.get(), buf, sizeof(buf));
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

    sockguard sock_;

    char buf_[4];
    size_t fill_;

    bool have_channel_;
    uint32_t size_;
    std::string channel_;
    std::string message_;
};

size_t ignore_all(char* ptr, size_t size, size_t nmemb, void* userdata) {
    return size * nmemb;
}

class Request {
public:
    Request(CURLM* multi, const std::string& url, const std::string& json)
        : multi_(multi), headers_(nullptr) {
        curl_ = curl_easy_init();
        curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_, CURLOPT_MAXREDIRS, 10L);
        curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl_, CURLOPT_POST, 1);
        headers_ = curl_slist_append(headers_,
                                     "Content-Type: application/json");
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers_);
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, json.size());
        curl_easy_setopt(curl_, CURLOPT_COPYPOSTFIELDS, json.data());

        curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, ignore_all);

        curl_multi_add_handle(multi_, curl_);
    }

    ~Request() {
        curl_multi_remove_handle(multi_, curl_);
        curl_easy_cleanup(curl_);
        curl_slist_free_all(headers_);
    }

    bool done() {
        long status;
        curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &status);
        return status != 0;
    }

private:
    CURL* curl_;
    CURLM* multi_;
    struct curl_slist* headers_;
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

    std::string url;

    CURLM* multi;

    std::vector<Request> requests;
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

    g_info.requests.emplace_back(g_info.multi, g_info.url, obj->str());
}

int run(const std::string& listener, int* fd) {
    openlog("sender", LOG_PID, LOG_DAEMON);

    if (curl_global_init(CURL_GLOBAL_ALL)) {
        syslog(LOG_ERR, "CURL failed to initialize");
        return EXIT_FAILURE;
    }
    g_info.multi = curl_multi_init();
    if (!g_info.multi) {
        syslog(LOG_ERR, "CURL did not to initialize");
        return EXIT_FAILURE;
    }

    std::vector<Client> clients;
    int still_running;
    int exitvalue;
    sockguard sock_;
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
            syslog(LOG_ERR, "Invalid host or port in: %s", listener.c_str());
            goto error;
        }
        for (auto ptr = res; ptr; ptr = ptr->ai_next) {
            sock_.reset(socket(ptr->ai_family, ptr->ai_socktype,
                               ptr->ai_protocol));
            if (!sock_) continue;
            if (bind(sock_.get(), res->ai_addr, res->ai_addrlen)) {
                sock_.reset();
                continue;
            }
            break;
        }
        freeaddrinfo(res);
        if (!sock_) {
            syslog(LOG_ERR, "Unable to bind: %s", listener.c_str());
            goto error;
        }
    } else {
        // socket
        sock_.reset(socket(PF_LOCAL, SOCK_STREAM, 0));
        if (!sock_) {
            syslog(LOG_ERR, "Unable to create a unix socket: %s",
                   strerror(errno));
            goto error;
        }
        struct sockaddr_un name;
        name.sun_family = AF_LOCAL;
        strncpy(name.sun_path, listener.c_str(), sizeof(name.sun_path));
        name.sun_path[sizeof(name.sun_path) - 1] = '\0';
        while (true) {
            if (bind(sock_.get(), reinterpret_cast<struct sockaddr*>(&name),
                     SUN_LEN(&name)) == 0) {
                break;
            }
            if (errno == EADDRINUSE) {
                if (unlink(listener.c_str()) == 0) {
                    continue;
                }
                errno = EADDRINUSE;
            }
            syslog(LOG_ERR, "Bind failed: %s", strerror(errno));
            goto error;
        }
    }

    if (listen(sock_.get(), 10)) {
        syslog(LOG_ERR, "Listen failed: %s", strerror(errno));
        goto error;
    }

    make_nonblocking(sock_.get());

    {
        int value = 1;
#ifdef SO_REUSEPORT
        setsockopt(sock_.get(), SOL_SOCKET, SO_REUSEPORT,
                   &value, sizeof(value));
#else
        setsockopt(sock_.get(), SOL_SOCKET, SO_REUSEADDR,
                   &value, sizeof(value));
#endif
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, quit);
    signal(SIGTERM, quit);

    while (true) {
        if (write(*fd, "", 1) != -1 || errno != EINTR) break;
    }
    close(*fd);
    *fd = -1;

    while (!g_quit) {
        curl_multi_perform(g_info.multi, &still_running);

        for (auto it = g_info.requests.begin(); it != g_info.requests.end();) {
            if (it->done()) {
                it = g_info.requests.erase(it);
            } else {
                ++it;
            }
        }

        fd_set read_set;
        fd_set write_set;
        fd_set err_set;
        int max = -1;
        FD_ZERO(&read_set);
        FD_ZERO(&write_set);
        FD_ZERO(&err_set);

        long timeout = -1;
        struct timeval *to = nullptr;
        struct timeval _to;
        curl_multi_timeout(g_info.multi, &timeout);
        if (timeout >= 0) {
            _to.tv_sec = timeout / 1000;
            _to.tv_usec = (timeout % 1000) * 1000;
            to = &_to;
        }
        if (curl_multi_fdset(g_info.multi, &read_set, &write_set, &err_set,
                             &max) != CURLM_OK) {
            syslog(LOG_ERR, "curl_multi_fdset failed");
            goto error;
        }

        max = std::max(max, sock_.get());
        FD_SET(sock_.get(), &read_set);
        for (auto it = clients.begin(); it != clients.end();) {
            if (it->sock() == -1) {
                it = clients.erase(it);
            } else {
                max = std::max(max, it->sock());
                FD_SET(it->sock(), &read_set);
                ++it;
            }
        }
        auto ret = select(max + 1, &read_set, &write_set, &err_set, to);
        if (ret < 0) {
            if (errno == EINTR) continue;
            syslog(LOG_ERR, "Select failed: %s", strerror(errno));
            goto error;
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
        if (ret > 0 && FD_ISSET(sock_.get(), &read_set)) {
            ret--;
            sockguard sock(accept(sock_.get(), nullptr, nullptr));
            if (!sock) {
                if (errno == EINTR) continue;
                if (errno == EWOULDBLOCK || errno == EAGAIN) continue;
                syslog(LOG_WARNING, "Accept failed: %s", strerror(errno));
            } else if (make_nonblocking(sock.get())) {
                if (clients.size() == MAX_CLIENTS) {
                    // Remove oldest
                    clients.erase(clients.begin());
                }
                clients.emplace_back(sock.release());
            }
        }
    }

    exitvalue = EXIT_SUCCESS;
    goto end;
 error:
    exitvalue = EXIT_FAILURE;

 end:
    clients.clear();
    g_info.requests.clear();
    curl_multi_cleanup(g_info.multi);
    curl_global_cleanup();
    unlink(listener.c_str());
    closelog();
    return exitvalue;
}

}  // namespace

int main() {
    auto cfg = Config::create();
    if (!cfg->load("./sender.config")) {
        cfg->load(SYSCONFDIR "/sender.config");
    }
    g_info.username = cfg->get("username", "stuff-sender-bot");
    g_info.url = cfg->get("url", "");
    if (g_info.url.empty()) {
        std::cerr << "No url configured" << std::endl;
        return EXIT_FAILURE;
    }
    g_info.icon_url = cfg->get("icon_url", "");
    g_info.icon_emoji = cfg->get("icon_emoji", "");
    auto const& listener = cfg->get("listener", "");
    if (listener.empty()) {
        std::cerr << "No listener configured" << std::endl;
        return EXIT_FAILURE;
    }
    cfg.reset();

    int fd[2];
    if (pipe(fd)) {
        std::cerr << "Unable to create pipe: " << strerror(errno) << std::endl;
        return EXIT_FAILURE;
    }

    auto pid = fork();
    if (pid < 0) {
        std::cerr << "Unable to fork: " << strerror(errno) << std::endl;
        close(fd[0]);
        close(fd[1]);
        return EXIT_FAILURE;
    }
    if (pid == 0) {
        close(fd[0]);
        setpgrp();
        if (listener.find(':') != std::string::npos ||
            listener.front() == '/') {
            chdir("/");
        }
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        int ret = run(listener, fd + 1);
        if (fd[1] != -1) {
            while (true) {
                if (write(fd[1], "", 1) != -1 || errno != EINTR) break;
            }
            close(fd[1]);
        }
        _exit(ret);
    } else {
        close(fd[1]);
        char c;
        while (true) {
            auto ret = read(fd[0], &c, 1);
            if (ret == 1) {
                break;
            }
            if (ret < 0 && errno == EINTR) {
                continue;
            }
            c = '1';
            break;
        }
        if (c) {
            std::cerr << "Failed to start, see syslog for details" << std::endl;
            close(fd[0]);
            return EXIT_FAILURE;
        }
        close(fd[0]);
        return EXIT_SUCCESS;
    }
}
