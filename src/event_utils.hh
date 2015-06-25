#ifndef EVENT_UTILS_HH
#define EVENT_UTILS_HH

#include <functional>
#include <memory>
#include <string>

namespace stuff {

class Config;
class Event;
class SenderClient;

class EventUtils {
public:
    virtual ~EventUtils();

    virtual std::unique_ptr<Event> create(
            const std::string& name, time_t start) = 0;
    virtual void created(Event* event) = 0;

    virtual std::vector<std::unique_ptr<Event>> all() = 0;
    virtual std::unique_ptr<Event> next() = 0;

    virtual bool good() const = 0;

    virtual void cancel(Event* event, size_t index) = 0;

    virtual void updated(Event* event, int64_t was_first) = 0;

    virtual void going(Event* event, bool going, const std::string& user,
                       const std::string& owner) = 0;

    virtual const std::string& channel() const = 0;

    static std::unique_ptr<EventUtils> create(
            const std::string& channel,
            std::function<void(const std::string&)> error_cb,
            Config* config,
            SenderClient* sender);

    static const double ONE_DAY_IN_SEC;
    static const double ONE_WEEK_IN_SEC;
    static const double ONE_YEAR_IN_SEC;

    static std::string format_date(time_t date);

protected:
    EventUtils();

private:
    EventUtils(const EventUtils&) = delete;
    EventUtils& operator=(const EventUtils&) = delete;
};

}  // namespace stuff

#endif /* EVENT_UTILS_HH */
