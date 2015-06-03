#ifndef CONFIG_HH
#define CONFIG_HH

#include <memory>
#include <string>

namespace stuff {

class Config {
public:
    virtual ~Config() {}

    virtual std::string get(const std::string& name,
                            const std::string& fallback) const = 0;
    virtual bool load(const std::string& path) = 0;

    static std::unique_ptr<Config> create();

protected:
    Config() {}

private:
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
};

}  // namespace stuff

#endif /* CONFIG_HH */
