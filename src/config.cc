#include "common.hh"

#include <fstream>
#include <unordered_map>

#include "config.hh"
#include "strutils.hh"

namespace stuff {

namespace {

class ConfigImpl : public Config {
public:
    ~ConfigImpl() override {
    }

    std::string get(const std::string& name,
                    const std::string& fallback) const override {
        auto it = data_.find(name);
        if (it == data_.end()) return fallback;
        return it->second;
    }

    bool load(const std::string& path) override {
        std::ifstream in(path);
        std::unordered_map<std::string, std::string> data;
        while (in.good()) {
            std::string line;
            std::getline(in, line);
            if (line.empty() || line.front() == '#') continue;
            auto pos = line.find('=');
            if (pos == std::string::npos) return false;
            data.insert(std::make_pair(trim(line.substr(0, pos)),
                                       trim(line.substr(pos + 1))));
        }
        if (!in.eof()) return false;
        data_.swap(data);
        return true;
    }

    ConfigImpl() {
    }

private:
    std::unordered_map<std::string, std::string> data_;
};

}  // namespace

std::unique_ptr<Config> Config::create() {
    return std::unique_ptr<Config>(new ConfigImpl());
}

}  // namespace stuff
