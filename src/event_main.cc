#include "common.hh"

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "args.hh"
#include "cgi.hh"
#include "db.hh"
#include "event.hh"
#include "fsutils.hh"
#include "http.hh"
#include "sqlite3_db.hh"

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

std::string g_db_path;

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
    if (!mkdir_p(g_db_path)) {
        Http::response(200, "Unable to create database directory");
        return nullptr;
    }
    auto db = SQLite3::open(g_db_path + tmp + ".db");
    if (!db || db->bad()) {
        Http::response(200, "Unable to open database");
        db.reset();
    } else if (!Event::setup(db.get())) {
        Http::response(200, "Unable to setup database");
        db.reset();
    }
    return std::move(db);
}

bool parse(const std::string& text, std::vector<std::string>* args) {
    if (Args::parse(text, args)) return true;
    Http::response(200, "Invalid parameter format");
    return false;
}

const double ONE_DAY_IN_SEC = 24.0 * 60.0 * 60.0;
const double ONE_WEEK_IN_SEC = ONE_DAY_IN_SEC * 7.0;
// It's OK that we ignore leap years here
const double ONE_YEAR_IN_SEC = 365 * ONE_DAY_IN_SEC;

std::string format_date(time_t date) {
    time_t now = time(NULL);
    struct tm _t;
    struct tm* t = localtime_r(&date, &_t);
    double diff = difftime(date, now);
    char tmp[100];
    if (diff <= ONE_DAY_IN_SEC) {
        // Same day, just show time
        strftime(tmp, sizeof(tmp), "%H:%M", t);
    } else if (diff <= ONE_WEEK_IN_SEC) {
        // Inside a week, show day and time
        strftime(tmp, sizeof(tmp), "%A %H:%M", t);
    } else if (diff <= ONE_YEAR_IN_SEC / 2.0) {
        // Inside a year, show date, day and time
        strftime(tmp, sizeof(tmp), "%A %d/%m %H:%M", t);
    } else {
        strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M", t);
    }
    return tmp;
}

void signal_channel(const std::string& str) {
    
}

void signal_event(const std::unique_ptr<Event>& event) {
    std::ostringstream ss;
    ss << event->name() << " @ " << format_date(event->start()) << std::endl;
    if (!event->text().empty()) {
        ss << event->text() << std::endl;
    }
    ss << std::endl;
    ss << "Use /going to join the event" << std::endl;
    signal_channel(ss.str());
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
        time_t tmp = mktime(&_tmp) + days * ONE_DAY_IN_SEC;
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

bool create(const std::string& channel,
            std::map<std::string, std::string>& data) {
    std::vector<std::string> args;
    if (!parse(data["text"], &args)) return true;
    if (args.size() < 2) {
        Http::response(200, "Usage: /create NAME START [TEXT]");
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
    auto db = open(channel);
    if (!db) return true;
    auto event = Event::create(db, name, start);
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
    auto next_event = Event::next(db);
    if (next_event->id() == event->id()) {
        signal_event(next_event);
    }
    return true;
}

template<typename Iterator>
bool append_indexes(Iterator begin, Iterator end,
                    std::vector<unsigned long>* out) {
    for (auto it = begin; it != end; ++it) {
        try {
            size_t end;
            auto tmp = std::stoul(*it, &end);
            if (end != it->size()) {
                Http::response(200, "Bad index: " + *it);
                return false;
            }
            out->push_back(tmp);
        } catch (std::invalid_argument& e) {
            Http::response(200, "Bad index: " + *it);
            return false;
        }
    }
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
        if (!append_indexes(args.begin(), args.end(), &indexes)) {
            return true;
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
        } else if (events.size() == 1) {
            Http::response(200, "There is only one event");
        } else {
            std::ostringstream ss;
            ss << "There are only " << events.size() << " events";
            Http::response(200, ss.str());
        }
        return true;
    }
    std::string signal;
    for (const auto& index : indexes) {
        if (index == 0) {
            std::ostringstream ss;
            ss << "Event canceled: " << events[index]->name() << " @ "
               << format_date(events[index]->start());
            signal = ss.str();
        }
        events[index]->remove();
    }
    if (indexes.size() > 1) {
        Http::response(200, "Events removed");
    } else {
        Http::response(200, "Event removed");
    }
    if (!signal.empty()) {
        signal_channel(signal);
    }
    return true;
}

bool update(const std::string& channel,
            std::map<std::string, std::string>& data) {
    std::set<std::string> update;
    update.insert("name");
    update.insert("start");
    update.insert("text");
    std::vector<std::string> args;
    if (!parse(data["text"], &args)) return true;
    if (args.empty()) {
        Http::response(200, "Usage: /update [INDEX] [name NAME] [start START] [text TEXT]");
        return true;
    }
    auto db = open(channel);
    if (!db) return true;
    std::unique_ptr<Event> event;
    auto it = args.begin();
    if (update.count(*it) == 0) {
        std::vector<unsigned long> indexes;
        if (!append_indexes(args.begin(), ++it, &indexes)) {
            return true;
        }
        auto events = Event::all(db);
        if (indexes.front() >= events.size()) {
            std::ostringstream ss;
            ss << "No such event: " << indexes.front() << std::endl;
            Http::response(200, ss.str());
            return true;
        }
        event.swap(events[indexes.front()]);
        ++it;
    } else {
        event = Event::next(db);
        if (!event) {
            Http::response(200, "No event to update");
            return true;
        }
    }
    auto first_event = Event::next(db)->id();
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
        auto next_event = Event::next(db);
        if (next_event->id() != first_event || next_event->id() == event->id()) {
            signal_event(next_event);
        }
    } else {
        Http::response(200, "Update failed");
    }
    return true;
}

bool show(const std::string& channel,
          std::map<std::string, std::string>& data) {
    std::vector<std::string> args;
    if (!parse(data["text"], &args)) return true;
    std::vector<unsigned long> indexes;
    if (args.empty()) {
        indexes.push_back(0);
    } else {
        if (!append_indexes(args.begin(), args.end(), &indexes)) {
            return true;
        }
    }
    auto db = open(channel);
    if (!db) return true;
    auto events = Event::all(db);
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
               << format_date(events[index]->start()) << std::endl;
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

bool going(const std::string& channel,
           std::map<std::string, std::string>& data,
           bool going) {
    std::vector<std::string> args;
    const auto& user_name = data["user_name"];
    if (user_name.empty()) {
        Http::response(500, "No user_name");
        return true;
    }
    if (!parse(data["text"], &args)) return true;
    std::unique_ptr<Event> event;
    std::vector<unsigned long> indexes;
    std::string note;
    auto db = open(channel);
    if (!db) return true;
    if (args.empty()) {
        event = Event::next(db);
    } else if (args.size() == 1) {
        try {
            size_t end;
            auto tmp = std::stoul(args.front(), &end);
            if (end == args.front().size()) {
                indexes.push_back(tmp);
            }
        } catch (std::invalid_argument& e) {
        }

        if (indexes.empty()) {
            event = Event::next(db);
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
    if (!event) {
        auto events = Event::all(db);
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
    event->update_going(user_name, going, note);
    if (event->store()) {
        Http::response(200, "Your wish have been recorded, if not granted");
        auto next_event = Event::next(db);
        if (next_event->id() == event->id()) {
            if (going) {
                signal_channel(user_name + " will be attending " +
                               event->name());
            } else {
                signal_channel(user_name + " will not be attending " +
                               event->name());
            }
        }
    } else {
        Http::response(200, "Event store failed");
    }
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

int main(int argc, char** argv) {
    if (argc < 2) {
        g_db_path = LOCALSTATEDIR "/";
    } else if (argc == 2) {
        g_db_path = argv[1];
        if (g_db_path.empty()) {
            g_db_path = ".";
        } else if (g_db_path.back() != '/') {
            g_db_path.push_back('/');
        }
    } else {
        std::cerr << "Too many arguments" << std::endl;
        return EXIT_FAILURE;
    }
    return CGI::run(handle_request);
}