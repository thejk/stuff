#include "common.hh"

#include "db.hh"
#include "event.hh"

namespace stuff {

namespace {

const std::string kEventTable = "events";
const std::string kEventGoingTable = "events_going";

class EventImpl : public Event {
public:
    ~EventImpl() override {
    }

    const std::string& name() const override {
        return name_;
    }
    void set_name(const std::string& name) override {
        if (name_ == name) return;
        edit();
        editor_->set("name", name);
        name_ = name;
    }

    const std::string& text() const override {
        return text_;
    }
    void set_text(const std::string& text) override {
        if (text_ == text) return;
        edit();
        editor_->set("text", text);
        text_ = text;
    }

    time_t start() const override {
        return start_;
    }
    void set_start(time_t start) override {
        if (start_ == start) return;
        edit();
        editor_->set("start", static_cast<int64_t>(start));
        start_ = start;
    }

    void going(std::set<std::string>* going,
               std::set<std::string>* not_going) const override {
        if (going) {
            going->clear();
            going->insert(going_.begin(), going_.end());
        }
        if (not_going) {
            not_going->clear();
            not_going->insert(not_going_.begin(), not_going_.end());
        }
    }

    bool is_going(const std::string& name) const override {
        return going_.count(name) > 0;
    }

    void update_going(const std::string& name, bool going) override {
        auto it = going_.find(name);
        if (it != going_.end()) {
            if (going) return;
            going_.erase(it);
            not_going_.insert(name);
            going_uptodate_ = false;
        } else {
            it = not_going_.find(name);
            if (it != not_going_.end()) {
                if (!going) return;
                going_.insert(name);
                not_going_.erase(it);
                going_uptodate_ = false;
            } else {
                if (going) {
                    going_.insert(name);
                } else {
                    not_going_.insert(name);
                }
            }
        }
        going_uptodate_ = false;
    }

    bool store() override {
        if (editor_) {
            DB::Transaction transaction(db_);
            if (!editor_->commit()) return false;
            if (new_) {
                id_ = editor_->last_insert_rowid();
            }
            if (store_going() && transaction.commit()) {
                new_ = false;
                editor_.reset();
                return true;
            } else {
                if (new_) id_ = 0;
                return false;
            }
        } else {
            if (new_) return false;
            return store_going();
        }
    }

    bool remove() override {
        if (new_) return true;
        DB::Transaction transaction(db_);
        if (!db_->remove(kEventTable,
                         DB::Condition("id", DB::Condition::EQUAL, id_)))
            return false;
        if (!db_->remove(kEventGoingTable,
                         DB::Condition("event", DB::Condition::EQUAL, id_)))
            return false;
        if (transaction.commit()) {
            new_ = true;
            id_ = 0;
            return true;
        }
        return false;
    }

    bool load(DB::Snapshot* snapshot) {
        editor_.reset();
        new_ = false;
        if (snapshot->bad()) return false;
        if (!snapshot->get(0, &id_)) return false;
        if (!snapshot->get(1, &name_)) return false;
        int64_t tmp;
        if (!snapshot->get(2, &tmp)) return false;
        start_ = tmp;
        if (!snapshot->get(3, &text_)) {
            text_ = "";
        }
        return load_going();
    }

    EventImpl(std::shared_ptr<DB> db)
        : db_(db), id_(0), new_(true), going_uptodate_(true) {
    }

private:
    void edit() {
        if (editor_) return;
        if (new_) {
            editor_ = db_->insert(kEventTable);
        } else {
            editor_ = db_->update(kEventTable,
                                  DB::Condition("id",
                                                DB::Condition::EQUAL,
                                                DB::Value(id_)));
        }
    }

    bool store_going(const std::set<std::string>& names, bool going) const {
        for (const auto& name : names) {
            auto editor = db_->insert(kEventGoingTable);
            editor->set("event", id_);
            editor->set("name", name);
            editor->set("going", going);
            if (!editor->commit()) {
                return false;
            }
        }
        return true;
    }

    bool store_going() {
        if (going_uptodate_) return true;
        DB::Transaction transaction(db_);
        if (!db_->remove(kEventGoingTable,
                         DB::Condition("event", DB::Condition::EQUAL, id_)))
            return false;
        if (!store_going(going_, true) || !store_going(not_going_, false))
            return false;
        if (transaction.commit()) {
            going_uptodate_ = true;
            return true;
        }
        return false;
    }
    bool load_going() {
        going_.clear();
        not_going_.clear();
        going_uptodate_ = true;
        auto snapshot = db_->select(kEventGoingTable,
                                    DB::Condition("event",
                                                  DB::Condition::EQUAL,
                                                  id_));
        if (!snapshot) return true;
        do {
            bool is_going;
            std::string name;
            if (!snapshot->get(0, &name) || !snapshot->get(1, &is_going))
                return false;
            if (is_going) {
                going_.insert(name);
            } else {
                not_going_.insert(name);
            }
        } while (snapshot->next());
        return !snapshot->bad();
    }

    std::shared_ptr<DB> db_;
    std::shared_ptr<DB::Editor> editor_;
    int64_t id_;
    std::string name_;
    std::string text_;
    time_t start_;
    bool new_;
    bool going_uptodate_;
    std::set<std::string> going_;
    std::set<std::string> not_going_;
};

}  // namespace

// static
bool Event::setup(DB* db) {
    DB::Declaration decl;
    decl.push_back(std::make_pair("id", DB::PrimaryKey(DB::Type::INT64)));
    decl.push_back(std::make_pair("name", DB::NotNull(DB::Type::STRING)));
    decl.push_back(std::make_pair("start", DB::NotNull(DB::Type::INT64)));
    decl.push_back(std::make_pair("text", DB::Type::STRING));
    if (!db->insert_table(kEventTable, decl)) return false;
    decl.clear();
    decl.push_back(std::make_pair("event", DB::NotNull(DB::Type::INT64)));
    decl.push_back(std::make_pair("name", DB::NotNull(DB::Type::STRING)));
    decl.push_back(std::make_pair("going", DB::NotNull(DB::Type::BOOL)));
    return db->insert_table(kEventGoingTable, decl);
}

// static
std::unique_ptr<Event> Event::next(std::shared_ptr<DB> db) {
    auto snapshot =
        db->select(kEventTable, DB::Condition(), DB::OrderBy("start"));
    if (snapshot) {
        do {
            auto ev = new EventImpl(db);
            if (ev->load(snapshot.get())) {
                return std::unique_ptr<Event>(ev);
            }
            delete ev;
        } while (snapshot->next());
    }
    return nullptr;
}

// static
std::vector<std::unique_ptr<Event>> Event::all(std::shared_ptr<DB> db) {
    auto snapshot =
        db->select(kEventTable, DB::Condition(), DB::OrderBy("start"));
    std::vector<std::unique_ptr<Event>> ret;
    if (snapshot) {
        do {
            auto ev = new EventImpl(db);
            if (ev->load(snapshot.get())) {
                ret.emplace_back(ev);
            } else {
                delete ev;
            }
        } while (snapshot->next());
    }
    return ret;
}

// static
std::unique_ptr<Event> Event::create(std::shared_ptr<DB> db,
                                     const std::string& name, time_t start) {
    std::unique_ptr<Event> ev(new EventImpl(db));
    ev->set_name(name);
    ev->set_start(start);
    return ev;
}


}  // namespace stuff
