#ifndef SENDER_CLIENT_HH
#define SENDER_CLIENT_HH

#include <memory>
#include <string>

namespace stuff {

class Config;

class SenderClient {
public:
    class Error {
    public:
        virtual ~Error() {}

        virtual void error(const std::string& message) = 0;
        virtual void error(const std::string& message, int error) = 0;

    protected:
        Error() {}
    };

    virtual ~SenderClient() {}

    virtual void send(const std::string& channel,
                      const std::string& message) = 0;

    static std::unique_ptr<SenderClient> create(
            const Config* config, std::shared_ptr<Error> error = nullptr);

protected:
    SenderClient() {}

private:
    SenderClient(const SenderClient&) = delete;
    SenderClient& operator=(const SenderClient&) = delete;
};

}  // namespace stuff

#endif /* SENDER_CLIENT_HH */
