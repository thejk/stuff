#ifndef SOCKUTILS_HH
#define SOCKUTILS_HH

namespace stuff {

bool make_nonblocking(int sock);

bool calc_timeout(const struct timeval* target, struct timeval* timeout);

class sockguard {
public:
    sockguard()
        : sock_(-1) {
    }
    explicit sockguard(int sock)
        : sock_(sock) {
    }
    sockguard(sockguard&& sock)
        : sock_(sock.sock_) {
    }
    ~sockguard() {
        reset();
    }
    sockguard& operator=(sockguard&& sock) {
        reset(sock.sock_);
        return *this;
    }
    void reset() {
        if (sock_ != -1) {
            close(sock_);
            sock_ = -1;
        }
    }
    void reset(int sock) {
        if (sock_ != -1 && sock_ != sock) {
            close(sock_);
        }
        sock_ = sock;
    }
    void swap(sockguard& sock) {
        auto tmp = sock.sock_;
        sock.sock_ = sock_;
        sock_ = tmp;
    }
    operator bool() const {
        return sock_ != -1;
    }
    int get() const {
        return sock_;
    }
    int release() {
        auto ret = sock_;
        sock_ = -1;
        return ret;
    }

protected:
    sockguard(const sockguard&) = delete;
    sockguard& operator=(const sockguard&) = delete;
    static void close(int sock);

private:
    int sock_;
};

}  // namespace stuff

namespace std {
void swap(stuff::sockguard& s1, stuff::sockguard& s2) noexcept;
}  // namespace std

#endif /* SOCKUTILS_HH */
