#include "common.hh"

#include <algorithm>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "args.hh"
#include "cgi.hh"
#include "event.hh"
#include "db.hh"
#include "http.hh"
#include "sqlite3_db.hh"

#define DB_PATH "/var/stuff/"

    /*
token=gIkuvaNzQIHg97ATvDxqgjtO
team_id=T0001
team_domain=example
channel_id=C2147483705
channel_name=test
user_id=U2147483697
user_name=Steve
command=/weather
text=94070
        */

using namespace stuff;

namespace {

std::shared_ptr<DB> open(const std::string& channel) {
    std::string tmp = channel;
    for (auto it = tmp.begin(); it != tmp.end(); ++it) {
        if (!((*it >= 'a' && *it <= 'z') ||
              (*it >= 'A' && *it <= 'Z') ||
              (*it >= '0' && *it <= '9') ||
              *it == '-' || *it == '_' || *it == '.')) {
            *it = '.';
        }
    }
    auto db = SQLite3::open(DB_PATH + tmp + ".db");
    if (!db) {
        Http::response(200, "Unable to open database");
    }
    return std::move(db);
}

bool parse(const std::string& text, std::vector<std::string>* args) {
    if (Args::parse(text, args)) return true;
    Http::response(200, "Invalid parameter format");
    return false;
}

bool create(const std::string& channel,
            std::map<std::string, std::string>& data) {
    std::vector<std::string> args;
    if (!parse(data["text"], &args) || args.empty()) return true;
    
    return true;
}

bool cancel(const std::string& channel,
            std::map<std::string, std::string>& data) {
    std::vector<std::string> args;
    if (!parse(data["text"], &args)) return true;
    std::vector<unsigned long> indexes;
    if (args.empty()) {
        indexes.push_back(0);
    } else {
        for (const auto& arg : args) {
            try {
                size_t end;
                auto tmp = std::stoul(arg, &end);
                if (end != arg.size()) {
                    Http::response(200, "Bad index: " + arg);
                    return true;
                }
                indexes.push_back(tmp);
            } catch (std::invalid_argument& e) {
                Http::response(200, "Bad index: " + arg);
                return true;
            }
        }
        std::sort(indexes.begin(), indexes.end(),
                  std::greater<unsigned long>());
        std::unique(indexes.begin(), indexes.end());
    }
    auto db = open(channel);
    if (!db) return true;
    auto events = Event::all(db);
    if (indexes.front() >= events.size()) {
        if (events.empty()) {
            Http::response(200, "There are no events");
        } else {
            std::ostringstream ss;
            ss << "There are only " << events.size() << " events";
            Http::response(200, ss.str());
        }
        return true;
    }
    bool signal_channel = false;
    for (const auto& index : indexes) {
        if (index == 0) signal_channel = true;
        events[index]->remove();
    }
    if (indexes.size() > 1) {
        Http::response(200, "Events removed");
    } else {
        Http::response(200, "Event removed");
    }
    if (signal_channel) {
        
    }
    return true;
}

bool update(const std::string& channel,
            std::map<std::string, std::string>& data) {
    std::vector<std::string> args;
    if (!parse(data["text"], &args)) return true;
    
    return true;
}

bool show(const std::string& channel,
          std::map<std::string, std::string>& data) {
    std::vector<std::string> args;
    if (!parse(data["text"], &args)) return true;
    
    return true;
}

bool going(const std::string& channel,
           std::map<std::string, std::string>& data,
           bool going) {
    std::vector<std::string> args;
    if (!parse(data["text"], &args)) return true;
    
    return true;
}

bool handle_request(CGI* cgi) {
    switch (cgi->request_type()) {
    case CGI::GET:
    case CGI::POST:
        break;
    default:
        Http::response(500, "Unsupported request");
        return true;
    }

    std::map<std::string, std::string> data;
    cgi->get_data(&data);
    const auto& channel = data["channel"];
    if (channel.empty()) {
        Http::response(500, "No channel");
        return true;
    }
    auto command = data["command"];
    if (command.front() == '/') command = command.substr(1);
    if (command == "create") {
        return create(channel, data);
    }
    if (command == "cancel") {
        return cancel(channel, data);
    }
    if (command == "update") {
        return update(channel, data);
    }
    if (command == "show") {
        return show(channel, data);
    }
    if (command == "going") {
        return going(channel, data, true);
    }
    if (command == "!going") {
        return going(channel, data, false);
    }
    Http::response(500, "Unknown command: " + command);
    return true;
}

}  // namespace

int main() {
    return CGI::run(handle_request);
}
