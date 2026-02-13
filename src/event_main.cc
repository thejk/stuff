#include "common.hh"

#include <algorithm>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "args.hh"
#include "cgi.hh"
#include "config.hh"
#include "db.hh"
#include "event.hh"
#include "event_utils.hh"
#include "http.hh"
#include "sender_client.hh"

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

std::unique_ptr<Config> g_cfg;
std::unique_ptr<SenderClient> g_sender;

bool parse(const std::string& text, std::vector<std::string>* args) {
    if (Args::parse(text, args)) return true;
    Http::response(200, "Invalid parameter format");
    return false;
}

bool parse_time(const std::string& value, time_t* date) {
    struct tm _t, _tmp;
    time_t now = time(NULL);
    localtime_r(&now, &_t);
    _tmp = _t;
    auto ptr = strptime(value.c_str(), "%H:%M", &_tmp);
    if (ptr && !*ptr) goto done;
    _tmp = _t;
    ptr = strptime(value.c_str(), "%A %H:%M", &_tmp);
    if (ptr && !*ptr) {
        // Given the weekday, figure out distance to "now"
        int days = _tmp.tm_wday - _t.tm_wday;
        if (days <= 0) {
            days += 7;
        }
        time_t tmp = mktime(&_tmp) + days * EventUtils::ONE_DAY_IN_SEC;
        localtime_r(&tmp, &_tmp);
        goto done;
    }
    _tmp = _t;
    ptr = strptime(value.c_str(), "%d/%m %H:%M", &_tmp);
    if (ptr && !*ptr) goto done;
    _tmp = _t;
    ptr = strptime(value.c_str(), "%A %d/%m %H:%M", &_tmp);
    if (ptr && !*ptr) goto done;
    _tmp = _t;
    ptr = strptime(value.c_str(), "%Y-%m-%d %H:%M", &_tmp);
    if (ptr && !*ptr) goto done;
    return false;
 done:
    *date = mktime(&_tmp);
    return true;
}

bool create(EventUtils* utils,
            std::map<std::string, std::string>& data,
            std::vector<std::string>& args) {
    if (args.size() < 2) {
        Http::response(200, "Usage: create NAME START [TEXT]\n"
                       "> START can be either in YYYY-MM-DD HH:MM format "
                       "or DAY HH:MM format (for example Wednesday 17:30).\n");
        return true;
    }
    std::string name, text;
    time_t start;
    name = args.front();
    args.erase(args.begin());
    text = args.front();
    args.erase(args.begin());
    while (true) {
        if (parse_time(text, &start)) break;
        if (args.empty()) {
            Http::response(200, "Couldn't figure out when to start the event, try [DAY|DATE] HH:MM");
            return true;
        }
        text.push_back(' ');
        text.append(args.front());
        args.erase(args.begin());
    }
    auto event = utils->create(name, start);
    if (!utils->good()) return true;
    if (!event) {
        Http::response(200, "Unable to create event");
        return true;
    }
    if (!args.empty()) {
        std::string text(args.front());
        args.erase(args.begin());
        for (const auto& arg : args) {
            text.push_back(' ');
            text.append(arg);
        }
        event->set_text(text);
    }
    if (!event->store()) {
        Http::response(200, "Unable to store event");
        return true;
    }
    Http::response(200, "Event created");
    utils->created(event.get());
    return true;
}

template<typename Iterator>
bool append_indexes(Iterator begin, Iterator end,
                    std::vector<unsigned long>* out) {
    for (auto it = begin; it != end; ++it) {
        char* end = nullptr;
        errno = 0;
        auto tmp = strtoul(it->c_str(), &end, 10);
        if (errno || !end || *end) {
            Http::response(200, "Bad index: " + *it);
            return false;
        }
        out->push_back(tmp);
    }
    return true;
}

bool cancel(EventUtils* utils,
            std::map<std::string, std::string>& data,
            std::vector<std::string>& args) {
    std::vector<unsigned long> indexes;
    if (args.empty()) {
        indexes.push_back(0);
    } else {
        if (!append_indexes(args.begin(), args.end(), &indexes)) {
            return true;
        }
        std::sort(indexes.begin(), indexes.end(),
                  std::greater<unsigned long>());
        std::unique(indexes.begin(), indexes.end());
    }
    auto events = utils->all();
    if (!utils->good()) return true;
    if (indexes.front() >= events.size()) {
        if (events.empty()) {
            Http::response(200, "There are no events");
        } else if (events.size() == 1) {
            Http::response(200, "There is only one event");
        } else {
            std::ostringstream ss;
            ss << "There are only " << events.size() << " events";
            Http::response(200, ss.str());
        }
        return true;
    }
    for (const auto& index : indexes) {
        utils->cancel(events[index].get(), index);
    }
    if (indexes.size() > 1) {
        Http::response(200, "Events removed");
    } else {
        Http::response(200, "Event removed");
    }
    return true;
}

bool update(EventUtils* utils,
            std::map<std::string, std::string>& data,
            std::vector<std::string>& args) {
    std::set<std::string> update;
    update.insert("name");
    update.insert("start");
    update.insert("text");
    if (args.empty()) {
        Http::response(200, "Usage: update [INDEX] [name NAME] [start START] [text TEXT]");
        return true;
    }
    std::unique_ptr<Event> event;
    int64_t first_event;
    auto it = args.begin();
    if (update.count(*it) == 0) {
        std::vector<unsigned long> indexes;
        if (!append_indexes(args.begin(), ++it, &indexes)) {
            return true;
        }
        auto events = utils->all();
        if (!utils->good()) return true;
        if (indexes.front() >= events.size()) {
            std::ostringstream ss;
            ss << "No such event: " << indexes.front() << std::endl;
            Http::response(200, ss.str());
            return true;
        }
        first_event = events.front()->id();
        event.swap(events[indexes.front()]);
    } else {
        event = utils->next();
        if (!utils->good()) return true;
        if (!event) {
            Http::response(200, "No event to update");
            return true;
        }
        first_event = event->id();
    }
    while (it != args.end()) {
        if (*it == "name") {
            if (++it == args.end()) {
                Http::response(200, "Missing argument to name");
                return true;
            }
            event->set_name(*it);
            ++it;
        } else if (*it == "start") {
            if (++it == args.end()) {
                Http::response(200, "Missing argument to start");
                return true;
            }
            std::string text(*it);
            for (++it; it != args.end() && update.count(*it) == 0; ++it) {
                text.push_back(' ');
                text.append(*it);
            }
            time_t time;
            if (!parse_time(text, &time)) {
                Http::response(200, "Bad argument to start: " + text);
                return true;
            }
            event->set_start(time);
        } else if (*it == "text") {
            if (++it == args.end()) {
                event->set_text("");
            } else {
                std::string text(*it);
                for (++it; it != args.end() && update.count(*it) == 0; ++it) {
                    text.push_back(' ');
                    text.append(*it);
                }
                event->set_text(text);
            }
        } else {
            Http::response(200, "Expected name/start/text, not: " + *it);
            return true;
        }
    }
    if (event->store()) {
        Http::response(200, "Event updated");
        utils->updated(event.get(), first_event);
    } else {
        Http::response(200, "Update failed");
    }
    return true;
}

bool show(EventUtils* utils,
          std::map<std::string, std::string>& data,
          std::vector<std::string>& args) {
    std::vector<unsigned long> indexes;
    if (args.empty()) {
        indexes.push_back(0);
    } else {
        if (!append_indexes(args.begin(), args.end(), &indexes)) {
            return true;
        }
    }
    auto events = utils->all();
    if (!utils->good()) return true;
    std::ostringstream ss;
    for (const auto& index : indexes) {
        if (indexes.size() > 1) {
            ss << '(' << index << ") ";
        }
        if (index >= events.size()) {
            if (events.empty()) {
                ss << "There are no events" << std::endl;
            } else {
                ss << "No such event: " << index << std::endl;
            }
        } else {
            ss << events[index]->name() << " @ "
               << EventUtils::format_date(events[index]->start()) << std::endl;
            const auto& text = events[index]->text();
            if (!text.empty()) {
                ss << text << std::endl;
            }
            std::vector<Event::Going> going;
            events[index]->going(&going);
            auto it = going.begin();
            for (; it != going.end(); ++it) {
                if (!it->is_going) break;
                ss << it->name;
                if (!it->note.empty()) {
                    ss << ": " << it->note;
                }
                ss << std::endl;
            }
            if (it != going.end()) ss << std::endl;
            for (; it != going.end(); ++it) {
                ss << it->name << ": not going";
                if (!it->note.empty()) {
                    ss << ", " << it->note;
                }
                ss << std::endl;
            }
        }
    }
    Http::response(200, ss.str());
    return true;
}

bool going(EventUtils* utils,
           std::map<std::string, std::string>& data,
           std::vector<std::string>& args,
           bool going) {
    const auto& user_name = data["user_name"];
    if (user_name.empty()) {
        Http::response(500, "No user_name");
        return true;
    }
    std::unique_ptr<Event> event;
    std::vector<unsigned long> indexes;
    std::string note, user = user_name;
    if (!args.empty() && args.front() == "user") {
        if (args.size() == 1) {
            Http::response(200, "Expected username after 'user'");
            return true;
        }
        user = args[1];
        args.erase(args.begin(), args.begin() + 2);
    }
    if (args.empty()) {
        event = utils->next();
        if (!utils->good()) return true;
    } else {
        if (args.size() == 1) {
            char* end = nullptr;
            errno = 0;
            auto tmp = strtoul(args.front().c_str(), &end, 10);
            if (errno == 0 && end && !*end) {
                indexes.push_back(tmp);
            }

            if (indexes.empty()) {
                event = utils->next();
                if (!utils->good()) return true;
                note = args.front();
            }
        } else {
            auto it = args.begin() + 1;
            if (!append_indexes(args.begin(), it, &indexes)) {
                return true;
            }
            note = *it;
            for (++it; it != args.end(); ++it) {
                note.push_back(' ');
                note.append(*it);
            }
        }
    }
    if (!event) {
        auto events = utils->all();
        if (!utils->good()) return true;
        if (events.empty()) {
            Http::response(200, "There are no events to attend");
            return true;
        }
        if (indexes.front() >= events.size()) {
            std::ostringstream ss;
            ss << "There are no such event to attend: " << indexes.front();
            Http::response(200, ss.str());
            return true;
        }
        event.swap(events[indexes.front()]);
    }
    event->update_going(user, going, note);
    if (event->store()) {
        Http::response(200, "Your wish have been recorded, if not granted");
        utils->going(event.get(), going, user, user_name);
    } else {
        Http::response(200, "Event store failed");
    }
    return true;
}

bool help(std::vector<std::string>& args) {
    std::ostringstream ss;
    if (args.empty()) {
        ss << "Usage: help COMMAND" << std::endl;
        ss << "Known commands: create, update, cancel, show, going, !going, join, part";
    } else if (args.front() == "create") {
        ss << "Usage: create NAME START [TEXT]" << std::endl;
        ss << "Create a new event with the name NAME starting at START with"
           << " an optional description TEXT." << std::endl;
        ss << "START can be of the format: [DATE|DAY] HH:MM";
    } else if (args.front() == "update") {
        ss << "Usage: update [INDEX] [name NAME] [start START] [text TEXT]"
           << std::endl;
        ss << "Update an event, specified by index (default is next event)"
           << std::endl;
        ss << "See help for create for description of NAME, START and TEXT.";
    } else if (args.front() == "cancel") {
        ss << "Usage: cancel [INDEX...]" << std::endl;
        ss << "Cancel one or more events given by index"
           << " (default is next event)";
    } else if (args.front() == "show") {
        ss << "Usage: show [INDEX...]" << std::endl;
        ss << "Show one or more events given by index"
           << " (default is next event)";
    } else if (args.front() == "going" || args.front() == "join") {
        ss << "Usage: going [user USER] [INDEX] [NOTE]" << std::endl;
        ss << "Join an event specified by index (default is next event)"
           << " and add an optional NOTE" << std::endl;
        ss << "If USER is specified, you're saying that USER is joining"
           << " instead of yourself - use with caution" << std::endl;
        ss << "join is an alias for going";
    } else if (args.front() == "!going" || args.front() == "part") {
        ss << "Usage: !going [user USER] [INDEX] [NOTE]" << std::endl;
        ss << "Un-join an event specified by index (default is next event)"
           << " and add an optional NOTE" << std::endl;
        ss << "If USER is specified, you're saying that USER is not going"
           << " instead of yourself - use with caution" << std::endl;
        ss << "part is an alias for !going";
    } else {
        ss << "Unknown command: " << args.front();
    }
    Http::response(200, ss.str());
    return true;
}

void error_response(const std::string& message) {
    Http::response(200, message);
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
    const auto& token = data["token"];
    if (token != g_cfg->get("token", "")) {
        Http::response(500, "Bad token");
        return true;
    }
    const auto& channel = data["channel_name"];
    if (channel.empty()) {
        Http::response(500, "No channel");;
        return true;
    }
    std::vector<std::string> args;
    if (!parse(data["text"], &args)) return true;
    if (args.empty()) {
        std::ostringstream ss;
        ss << "Usage: [COMMAND] [ARGUMENTS]" << std::endl
           << "For more help about a command, use " << data["command"]
           << " help COMMAND";
        Http::response(200, ss.str());
        return true;
    }
    auto command = args.front();
    args.erase(args.begin());
    auto utils = EventUtils::create(channel, error_response, g_cfg.get(),
                                    g_sender.get());
    if (command == "create") {
        return create(utils.get(), data, args);
    }
    if (command == "cancel") {
        return cancel(utils.get(), data, args);
    }
    if (command == "update") {
        return update(utils.get(), data, args);
    }
    if (command == "show") {
        return show(utils.get(), data, args);
    }
    if (command == "going" || command == "join") {
        return going(utils.get(), data, args, true);
    }
    if (command == "!going" || command == "part") {
        return going(utils.get(), data, args, false);
    }
    if (command == "help") {
        return help(args);
    }
    Http::response(200, "Unknown command: " + command);
    return true;
}

}  // namespace

int main() {
    g_cfg = Config::create();
    if (!g_cfg->load("./event.config")) {
        g_cfg->load(SYSCONFDIR "/event.config");
    }
    g_sender = SenderClient::create(g_cfg.get());
    int ret = CGI::run(handle_request);
    g_sender.reset();
    g_cfg.reset();
    return ret;
}
