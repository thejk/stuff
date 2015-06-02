#ifndef EVENT_HH
#define EVENT_HH

#include <memory>
#include <set>
#include <string>
#include <vector>

namespace stuff {

class DB;

class Event {
public:
    virtual ~Event() {}

    virtual const std::string& name() const = 0;
    virtual void set_name(const std::string& name) = 0;

    virtual const std::string& text() const = 0;
    virtual void set_text(const std::string& text) = 0;

    virtual time_t start() const = 0;
    virtual void set_start(time_t start) = 0;

    virtual void going(std::set<std::string>* going,
                       std::set<std::string>* not_going) const = 0;
    virtual bool is_going(const std::string& name) const = 0;

    virtual void update_going(const std::string& name, bool going) = 0;

    virtual bool store() = 0;

    virtual bool remove() = 0;

    static bool setup(DB* db);

    static std::unique_ptr<Event> next(std::shared_ptr<DB> db);
    static std::vector<std::unique_ptr<Event>> all(std::shared_ptr<DB> db);
    static std::unique_ptr<Event> create(std::shared_ptr<DB> db,
                                         const std::string& name, time_t start);

protected:
    Event() { }
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
};

}  // namespace stuff

#endif /* EVENT_HH */
