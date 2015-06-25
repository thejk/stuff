#include "common.hh"

#include <sstream>

#include "config.hh"
#include "event.hh"
#include "event_utils.hh"
#include "fsutils.hh"
#include "sender_client.hh"
#include "sqlite3_db.hh"

namespace stuff {

namespace {

class EventUtilsImpl : public EventUtils {
public:
    EventUtilsImpl(const std::string& channel,
                   std::function<void(const std::string&)> error_cb,
                   Config* config, SenderClient* sender)
        : channel_(channel), error_cb_(error_cb), cfg_(config),
          sender_(sender) {
    }

    std::unique_ptr<Event> create(
            const std::string& name, time_t start) override {
        if (!db_ && !open()) return nullptr;
        return Event::create(db_, name, start);
    }

    void created(Event* event) override {
        if (!event) return;
        auto next_event = Event::next(db_);
        if (next_event->id() == event->id()) {
            signal_event(next_event);
        }
    }

    std::vector<std::unique_ptr<Event>> all() override {
        if (!db_ && !open()) return std::vector<std::unique_ptr<Event>>();
        return Event::all(db_);
    }

    std::unique_ptr<Event> next() override {
        if (!db_ && !open()) return nullptr;
        return Event::next(db_);
    }

    bool good() const override {
        return db_.get() != nullptr;
    }

    void cancel(Event* event, size_t index) override {
        if (!event) return;
        event->remove();
        if (index == 0) {
            std::ostringstream ss;
            ss << "Event canceled: " << event->name() << " @ "
               << format_date(event->start());
            signal_channel(ss.str());
        }
    }

    void updated(Event* event, int64_t was_first) override {
        auto next_event = Event::next(db_);
        if (next_event->id() != was_first ||
            next_event->id() == event->id()) {
            signal_event(next_event);
        }
    }

    void going(Event* event, bool going, const std::string& user,
               const std::string& owner) override {
        auto next_event = Event::next(db_);
        if (next_event->id() == event->id()) {
            std::string extra;
            if (user != owner) {
                extra = " says " + owner;
            }
            if (going) {
                signal_channel(user + " will be attending " +
                               event->name() + extra);
            } else {
                signal_channel(user + " will not be attending " +
                               event->name() + extra);
            }
        }
    }

    const std::string& channel() const override {
        return channel_;
    }

private:
    void signal_event(const std::unique_ptr<Event>& event) {
        std::ostringstream ss;
        ss << event->name() << " @ " << format_date(event->start()) << std::endl;
        if (!event->text().empty()) {
            ss << event->text() << std::endl;
        }
        ss << std::endl;
        ss << "Use /event going to join the event" << std::endl;
        signal_channel(ss.str());
    }

    void signal_channel(const std::string& str) {
        if (!sender_) return;
        sender_->send(channel_, str);
    }

    void error(const std::string& message) {
        if (error_cb_) {
            error_cb_(message);
        }
    }

    bool open() {
        std::string tmp = channel_;
        for (auto it = tmp.begin(); it != tmp.end(); ++it) {
            if (!((*it >= 'a' && *it <= 'z') ||
                  (*it >= 'A' && *it <= 'Z') ||
                  (*it >= '0' && *it <= '9') ||
                  *it == '-' || *it == '_' || *it == '.')) {
                *it = '.';
            }
        }
        std::string path;
        if (cfg_) path = cfg_->get("db_path", LOCALSTATEDIR);
        if (path.empty()) path = ".";
        if (!mkdir_p(path)) {
            error("Unable to create database directory");
            return false;
        }
        auto db = SQLite3::open(path + "/" + tmp + ".db");
        if (!db || db->bad()) {
            error("Unable to open database");
            return false;
        } else if (!Event::setup(db.get())) {
            error("Unable to setup database");
            return false;
        }
        db_ = std::move(db);
        return true;
    }

    std::string channel_;
    std::function<void(const std::string&)> error_cb_;
    std::shared_ptr<DB> db_;
    Config* cfg_;
    SenderClient* sender_;
};

}  // namespace

EventUtils::EventUtils() {
}

EventUtils::~EventUtils() {
}

std::unique_ptr<EventUtils> EventUtils::create(
        const std::string& channel,
        std::function<void(const std::string&)> error_cb,
        Config* config,
        SenderClient* sender) {
    std::unique_ptr<EventUtils> utils(
            new EventUtilsImpl(channel, error_cb, config, sender));
    return utils;
}

const double EventUtils::ONE_DAY_IN_SEC = 24.0 * 60.0 * 60.0;
const double EventUtils::ONE_WEEK_IN_SEC = ONE_DAY_IN_SEC * 7.0;
// It's OK that we ignore leap years here
const double EventUtils::ONE_YEAR_IN_SEC = 365 * ONE_DAY_IN_SEC;

std::string EventUtils::format_date(time_t date) {
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

}  // namespace stuff


