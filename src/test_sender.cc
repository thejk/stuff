#include "common.hh"

#include <cstdlib>
#include <cstring>
#include <iostream>

#include "config.hh"
#include "sender_client.hh"

using namespace stuff;

int main(int argc, char** argv) {
    class StdError : public SenderClient::Error {
    public:
        StdError() {
        }
        void error(const std::string& message) override {
            std::cerr << message << std::endl;
        }
        void error(const std::string& message, int error) override {
            std::cerr << message << ": " << strerror(error) << std::endl;
        }
    };

    if (argc != 4) {
        std::cerr << "Usage: `test_sender CONFIG CHANNEL MESSAGE`" << std::endl;
        return EXIT_FAILURE;
    }
    auto config = Config::create();
    if (!config->load(argv[1])) {
        std::cerr << "Error loading config: " << argv[1] << std::endl;
        return EXIT_FAILURE;
    }
    auto error = std::make_shared<StdError>();
    auto client = SenderClient::create(config.get(), error);
    if (!client) {
        return EXIT_FAILURE;
    }
    client->send(argv[2], argv[3]);
    return EXIT_SUCCESS;
}
