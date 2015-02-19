#include "common.hh"

#include "sqlite3_db.hh"

#include <map>
#include <sqlite3.h>

namespace stuff {

namespace {

class DeleteStmt {
public:
    void operator()(sqlite3_stmt* stmt) const {
        sqlite3_finalize(stmt);
    }
};

typedef std::unique_ptr<sqlite3_stmt,DeleteStmt> unique_stmt;

class DBImpl : public DB {
public:
    DBImpl()
        : db_(nullptr), bad_(true) {
    }

    ~DBImpl() {
        close();
    }

    void open(const std::string& path) {
        close();
        int err = sqlite3_open(path.c_str(), &db_);
        if (err == SQLITE_OK) {
            bad_ = false;
            prepare();
        } else {
            bad_ = true;
        }
    }

    void close() {
        if (!db_) return;
        unprepare();
        sqlite3_close(db_);
        db_ = nullptr;
    }

    bool insert_table(const std::string& table,
                      const Declaration& declaration) override {
        if (!db_) return false;
        std::string sql = "CREATE TABLE IF NOT EXISTS " + safe(table) + " (";
        std::string primary_key;
        bool first = true, pk_first = true;
        for (const auto& pair : declaration) {
            if (!first) sql += ','; else first = false;
            sql += safe(pair.first);
            if (pair.second.primary_key()) {
                if (!pk_first) primary_key += ','; else pk_first = false;
                primary_key += safe(pair.first);
            }
            switch (pair.second.type()) {
            case Type::STRING:
                sql += " TEXT";
                break;
            case Type::INT32:
            case Type::INT64:
            case Type::BOOL:
                sql += " INTEGER";
                break;
            case Type::DOUBLE:
                sql += " REAL";
                break;
            case Type::RAW:
                sql += " BLOB";
                break;
            }
            if (!pair.second.primary_key()) {
                if (pair.second.unique()) {
                    sql += " UNIQUE";
                }
                if (pair.second.not_null()) {
                    sql += " NOT NULL";
                }
            }
        }
        if (!primary_key.empty()) {
            sql += ", PRIMARY KEY (" + primary_key + ")";
        }
        sql += ")";
        unique_stmt stmt;
        if (!prepare(sql, &stmt)) return false;
        return exec(stmt);
    }

    bool remove_table(const std::string& table) override {
        if (!db_) return false;
        std::string sql = "DROP TABLE IF EXISTS " + safe(table);
        unique_stmt stmt;
        if (!prepare(sql, &stmt)) return false;
        return exec(stmt);
    }

    std::shared_ptr<Editor> insert(const std::string& table) override {
        return std::shared_ptr<Editor>(new InsertEditorImpl(this, table));
    }

    std::shared_ptr<Editor> update(const std::string& table,
                                   const Condition& condition) override {
        return std::shared_ptr<Editor>(
                new UpdateEditorImpl(this, table, condition));
    }

    std::shared_ptr<Snapshot> select(
            const std::string& table, const Condition& condition,
            const std::vector<OrderBy>& order_by) override {
        std::string sql = "SELECT * FROM " + safe(table);
        sql += compile(condition);
        sql += compile(order_by);
        unique_stmt stmt;
        if (!prepare(sql, &stmt)) return nullptr;
        int index = 1;
        if (!bind(stmt, condition, &index)) return nullptr;
        std::shared_ptr<Snapshot> ret(new SnapshotImpl(stmt));
        if (!ret->next()) return nullptr;
        return ret;
    }

    int64_t remove(const std::string& table,
                   const Condition& condition) override {
        std::string sql = "DELETE FROM " + safe(table) + compile(condition);
        unique_stmt stmt;
        if (!prepare(sql, &stmt)) return -1;
        int index = 1;
        if (!bind(stmt, condition, &index)) return -1;
        if (!exec(stmt)) return -1;
        return sqlite3_changes(db_);
    }

    bool start_transaction() override {
        return exec(stmt_begin_);
    }

    bool commit_transaction() override {
        return exec(stmt_commit_);
    }

    bool rollback_transaction() override {
        return exec(stmt_rollback_);
    }

    bool bad() override {
        return bad_;
    }

    std::string last_error() override {
        if (!db_) return "Out of memory";
        return sqlite3_errmsg(db_);
    }

    class EditorImpl : public Editor {
    public:
        EditorImpl(DBImpl* db, const std::string& table)
            : db_(db), table_(table), new_names_(true) {
        }

        void set(const std::string& name, const std::string& value) override {
            do_set(name, Value(value));
        }

        void set(const std::string& name, bool value) override {
            do_set(name, value);
        }

        void set(const std::string& name, double value) override {
            do_set(name, value);
        }

        void set(const std::string& name, int32_t value) override {
            do_set(name, value);
        }

        void set(const std::string& name, int64_t value) override {
            do_set(name, value);
        }

        void set_null(const std::string& name) override {
            do_set(name, nullptr);
        }

        void set(const std::string& name, const void* data,
                 size_t size) override {
            if (!data) {
                set(name, nullptr);
                blob_.erase(name);
            } else {
                auto ptr = reinterpret_cast<const char*>(data);
                auto vector = std::vector<char>(ptr, ptr + size);
                auto it = blob_.emplace(name, vector);
                if (!it.second) {
                    it.first->second.swap(vector);
                } else {
                    if (data_.erase(name) == 0) {
                        new_names_ = true;
                    }
                }
            }
        }

    protected:
        void do_set(const std::string& name, const Value&& value) {
            auto it = data_.emplace(name, value);
            if (!it.second) {
                it.first->second = value;
            } else {
                new_names_ = true;
            }
        }

        DBImpl* const db_;
        const std::string table_;
        std::map<std::string,Value> data_;
        std::map<std::string,std::vector<char>> blob_;
        bool new_names_;
    };

    class InsertEditorImpl : public EditorImpl {
    public:
        InsertEditorImpl(DBImpl* db, const std::string& table)
            : EditorImpl(db, table) {
        }

        bool commit() override {
            if (!stmt_ || new_names_) {
                if (!prepare()) return false;
                new_names_ = false;
            }
            int index = 1;
            for (const auto& pair : data_) {
                if (!db_->bind(stmt_, index++, pair.second)) return false;
            }
            for (const auto& pair : blob_) {
                if (!db_->bind(stmt_, index++, pair.second)) return false;
            }
            return db_->exec(stmt_);
        }

        int64_t last_insert_rowid() override {
            return sqlite3_last_insert_rowid(db_->db_);
        }

    private:
        bool prepare() {
            if (data_.empty() && blob_.empty()) return false;
            std::string sql = "INSERT INTO " + db_->safe(table_) + " (";
            for (const auto& pair : data_) {
                sql += db_->safe(pair.first) + ",";
            }
            for (const auto& pair : blob_) {
                sql += db_->safe(pair.first) + ",";
            }
            sql.pop_back();
            sql += ") VALUES (?";
            auto count = data_.size() + blob_.size() - 1;
            while (count--) {
                sql += ",?";
            }
            sql += ")";
            return db_->prepare(sql, &stmt_);
        }

        unique_stmt stmt_;
    };

    class UpdateEditorImpl : public EditorImpl {
    public:
        UpdateEditorImpl(DBImpl* db, const std::string& table,
                         const Condition& condition)
            : EditorImpl(db, table), condition_(condition) {
        }

        bool commit() override {
            if (!stmt_ || new_names_) {
                if (!prepare()) return false;
                new_names_ = false;
            }
            int index = 1;
            for (const auto& pair : data_) {
                if (!db_->bind(stmt_, index++, pair.second)) return false;
            }
            for (const auto& pair : blob_) {
                if (!db_->bind(stmt_, index++, pair.second)) return false;
            }
            if (!db_->bind(stmt_, condition_, &index)) return false;
            return db_->exec(stmt_);
        }

        int64_t last_insert_rowid() override {
            assert(false);
            return 0;
        }

    private:
        bool prepare() {
            if (data_.empty() && blob_.empty()) return false;
            std::string sql = "UPDATE " + db_->safe(table_) + " SET ";
            for (const auto& pair : data_) {
                sql += safe(pair.first) + "=?,";
            }
            for (const auto& pair : blob_) {
                sql += safe(pair.first) + "=?,";
            }
            sql.pop_back();
            sql += db_->compile(condition_);
            return db_->prepare(sql, &stmt_);
        }

        const Condition condition_;
        unique_stmt stmt_;
    };

    class SnapshotImpl : public Snapshot {
    public:
        SnapshotImpl(unique_stmt& stmt) {
            stmt_.swap(stmt);
        }
        bool get(const std::string& name, std::string* value) override {
            return get(find_column(name), value);
        }
        bool get(const std::string& name, bool* value) override {
            return get(find_column(name), value);
        }
        bool get(const std::string& name, double* value) override {
            return get(find_column(name), value);
        }
        bool get(const std::string& name, int32_t* value) override {
            return get(find_column(name), value);
        }
        bool get(const std::string& name, int64_t* value) override {
            return get(find_column(name), value);
        }
        bool get(const std::string& name,
                 std::vector<uint8_t>* value) override {
            return get(find_column(name), value);
        }
        bool is_null(const std::string& name, bool* value) override {
            return is_null(find_column(name), value);
        }

        bool get(uint32_t column, std::string* value) override {
            if (!value) { assert(false); return false; }
            if (!stmt_) return false;
            if (column >= static_cast<uint32_t>(
                        sqlite3_column_count(stmt_.get()))) return false;
            if (sqlite3_column_type(stmt_.get(), column) != SQLITE_TEXT) {
                return false;
            }
            value->assign(reinterpret_cast<const char*>(
                                  sqlite3_column_blob(stmt_.get(), column)),
                          sqlite3_column_bytes(stmt_.get(), column));
            return true;
        }
        bool get(uint32_t column, bool* value) override {
            if (!value) { assert(false); return false; }
            int32_t tmp;
            if (!get(column, &tmp)) return false;
            *value = tmp != 0;
            return true;
        }
        bool get(uint32_t column, double* value) override {
            if (!value) { assert(false); return false; }
            if (!stmt_) return false;
            if (column >= static_cast<uint32_t>(
                        sqlite3_column_count(stmt_.get()))) return false;
            if (sqlite3_column_type(stmt_.get(), column) != SQLITE_FLOAT) {
                return false;
            }
            *value = sqlite3_column_double(stmt_.get(), column);
            return true;
        }
        bool get(uint32_t column, int32_t* value) override {
            if (!value) { assert(false); return false; }
            if (!stmt_) return false;
            if (column >= static_cast<uint32_t>(
                        sqlite3_column_count(stmt_.get()))) return false;
            if (sqlite3_column_type(stmt_.get(), column) != SQLITE_INTEGER) {
                return false;
            }
            if (sizeof(int) >= sizeof(int32_t)) {
                *value = sqlite3_column_int(stmt_.get(), column);
            } else {
                *value = sqlite3_column_int64(stmt_.get(), column);
            }
            return true;
        }
        bool get(uint32_t column, int64_t* value) override {
            if (!value) { assert(false); return false; }
            if (!stmt_) return false;
            if (column >= static_cast<uint32_t>(
                        sqlite3_column_count(stmt_.get()))) return false;
            if (sqlite3_column_type(stmt_.get(), column) != SQLITE_INTEGER) {
                return false;
            }
            *value = sqlite3_column_int64(stmt_.get(), column);
            return true;
        }
        bool get(uint32_t column, std::vector<uint8_t>* value) override {
            if (!value) { assert(false); return false; }
            if (!stmt_) return false;
            if (column >= static_cast<uint32_t>(
                        sqlite3_column_count(stmt_.get()))) return false;
            if (sqlite3_column_type(stmt_.get(), column) != SQLITE_BLOB) {
                return false;
            }
            auto data = reinterpret_cast<const char*>(
                    sqlite3_column_blob(stmt_.get(), column));
            value->assign(data, data +
                          sqlite3_column_bytes(stmt_.get(), column));
            return true;
        }
        bool is_null(uint32_t column, bool* value) override {
            if (!value) { assert(false); return false; }
            if (!stmt_) return false;
            if (column >= static_cast<uint32_t>(
                        sqlite3_column_count(stmt_.get()))) return false;
            return sqlite3_column_type(stmt_.get(), column) == SQLITE_NULL;
        }

        bool next() override {
            if (!stmt_) return false;
            while (true) {
                switch (sqlite3_step(stmt_.get())) {
                case SQLITE_BUSY:
                    continue;
                case SQLITE_DONE:
                    stmt_.reset();
                    return false;
                case SQLITE_ROW:
                    return true;
                default:
                    stmt_.reset();
                    return false;
                }
            }
        }

        bool bad() override {
            return !stmt_;
        }

    private:
        uint32_t find_column(const std::string& name) {
#if SQLITE_ENABLE_COLUMN_METADATA
            auto ret = column_cache_.insert(std::make_pair(name, 0xfffffffful));
            if (!ret.second) return ret.first->second;
            const int count = sqlite3_column_count(stmt_.get());
            for (int i = 0; i < count; i++) {
                if (name.compare(sqlite3_column_origin_name(stmt_.get(),
                                                            i)) == 0) {
                    return ret.first->second = i;
                }
            }
#endif
            return 0xfffffffful;
        }

#if SQLITE_ENABLE_COLUMN_METADATA
        std::map<std::string,int> column_cache_;
#endif

        unique_stmt stmt_;
    };

    std::string compile(const Condition& condition) {
        if (condition.empty()) return "";
        std::string sql = " WHERE ";
        compile(sql, condition);
        return sql;
    }

    std::string compile(const std::vector<OrderBy>& order_by) {
        if (order_by.empty()) return "";
        std::string sql = " ORDER BY ";
        compile(sql, order_by);
        return sql;
    }

    bool bind(unique_stmt& stmt, int index, const std::string& text) {
        return sqlite3_bind_text(stmt.get(), index, text.data(), text.size(),
                                 SQLITE_STATIC) == SQLITE_OK;
    }

    bool bind(unique_stmt& stmt, int index, int32_t value) {
        if (sizeof(int) >= sizeof(int32_t)) {
            return sqlite3_bind_int(stmt.get(), index, value) == SQLITE_OK;
        } else {
            return sqlite3_bind_int64(stmt.get(), index, value) == SQLITE_OK;
        }
    }

    bool bind(unique_stmt& stmt, int index, int64_t value) {
        return sqlite3_bind_int64(stmt.get(), index, value) == SQLITE_OK;
    }

    bool bind(unique_stmt& stmt, int index, bool value) {
        return sqlite3_bind_int(stmt.get(), index, value ? 1 : 0) == SQLITE_OK;
    }

    bool bind(unique_stmt& stmt, int index, double value) {
        return sqlite3_bind_double(stmt.get(), index, value) == SQLITE_OK;
    }

    bool bind(unique_stmt& stmt, int index, const Value& value) {
        switch (value.type()) {
        case Type::STRING:
            return bind(stmt, index, value.string());
        case Type::INT32:
            return bind(stmt, index, value.i32());
        case Type::INT64:
            return bind(stmt, index, value.i64());
        case Type::BOOL:
            return bind(stmt, index, value.b());
        case Type::DOUBLE:
            return bind(stmt, index, value.d());
        case Type::RAW:
            return bind(stmt, index, nullptr);
        }
        assert(false);
        return false;
    }

    bool bind(unique_stmt& stmt, int index, std::nullptr_t) {
        return sqlite3_bind_null(stmt.get(), index) == SQLITE_OK;
    }

    bool bind(unique_stmt& stmt, int index, const std::vector<char>& blob) {
        return sqlite3_bind_blob64(stmt.get(), index, blob.data(),
                                   blob.size(), SQLITE_STATIC) == SQLITE_OK;
    }

    bool bind(unique_stmt& stmt, const Condition& condition, int* index) {
        switch (condition.mode()) {
        case Condition::NOOP:
            return true;
        case Condition::BOOL_BINARY:
            return bind(stmt, condition.c1(), index) &&
                bind(stmt, condition.c2(), index);
        case Condition::BOOL_UNARY:
            return bind(stmt, condition.c1(), index);
        case Condition::COMP_BINARY:
            return bind(stmt, (*index)++, condition.value());
        case Condition::COMP_UNARY:
            return true;
        }
        assert(false);
        return false;
    }

    void compile(std::string& sql, const Condition& condition) {
        switch (condition.mode()) {
        case Condition::NOOP:
            break;
        case Condition::BOOL_BINARY:
            sql += '(';
            compile(sql, condition.c1());
            switch (condition.bool_binary_op()) {
            case Condition::AND:
                sql += " && ";
                break;
            case Condition::OR:
                sql += " || ";
                break;
            }
            compile(sql, condition.c2());
            sql += ')';
            break;
        case Condition::BOOL_UNARY:
            switch (condition.bool_unary_op()) {
            case Condition::NOT:
                sql += " NOT ";
                break;
            }
            compile(sql, condition.c1());
            break;
        case Condition::COMP_BINARY:
            sql += "(" + safe(condition.column().name());
            switch (condition.binary_op()) {
            case Condition::EQUAL:
                sql += " == ";
                break;
            case Condition::NOT_EQUAL:
                sql += " != ";
                break;
            case Condition::LESS_THAN:
                sql += " < ";
                break;
            case Condition::GREATER_THAN:
                sql += " < ";
                break;
            case Condition::LESS_EQUAL:
                sql += " <= ";
                break;
            case Condition::GREATER_EQUAL:
                sql += " >= ";
                break;
            }
            sql += "?)";
            break;
        case Condition::COMP_UNARY:
            switch (condition.unary_op()) {
            case Condition::NEGATIVE:
                sql += "-" + safe(condition.column().name());
                break;
            case Condition::IS_NULL:
                sql += safe(condition.column().name()) + " ISNULL";
                break;
            }
            break;
        }
    }

    void compile(std::string& sql, const std::vector<OrderBy>& order_by) {
        bool first = true;
        for (const auto& entry : order_by) {
            if (!first) {
                sql += ',';
            } else {
                first = false;
            }
            sql += safe(entry.name());
            if (!entry.ascending()) {
                sql += " DESC";
            }
        }
    }

    bool exec(const unique_stmt& stmt) {
        if (!db_ || !stmt) return false;
        while (true) {
            switch (sqlite3_step(stmt.get())) {
            case SQLITE_BUSY:
                // Retry
                continue;
            case SQLITE_DONE:
                sqlite3_reset(stmt.get());
                return true;
            case SQLITE_ERROR:
            default:
                // Error
                sqlite3_reset(stmt.get());
                return false;
            }
        }
    }

    void prepare() {
        static const char* const statements = "BEGIN;COMMIT;ROLLBACK";
        const char* ptr = statements;
        if (!prepare(ptr, &stmt_begin_, &ptr) ||
            !prepare(ptr, &stmt_commit_, &ptr) ||
            !prepare(ptr, &stmt_rollback_, &ptr)) {
            bad_ = true;
            return;
        }
    }

    void unprepare() {
        stmt_begin_.reset();
        stmt_commit_.reset();
        stmt_rollback_.reset();
    }

    bool prepare(const char* str, unique_stmt* stmt, const char** tail) {
        assert(db_);
        sqlite3_stmt* ptr;
        if (sqlite3_prepare_v2(db_, str, -1, &ptr, tail) != SQLITE_OK) {
            stmt->reset();
            return false;
        }
        stmt->reset(ptr);
        return true;
    }

    bool prepare(const std::string& str, unique_stmt* stmt) {
        assert(db_);
        sqlite3_stmt* ptr;
        if (sqlite3_prepare_v2(db_, str.data(), str.size(), &ptr, nullptr) !=
            SQLITE_OK) {
            stmt->reset();
            return false;
        }
        stmt->reset(ptr);
        return true;
    }

    static const std::string& safe(const std::string& str) {
        return str;
    }

    sqlite3 *db_;
    bool bad_;
    unique_stmt stmt_begin_;
    unique_stmt stmt_commit_;
    unique_stmt stmt_rollback_;
};

}  // namespace

// static
DB* SQLite3::open(const std::string& path) {
    std::unique_ptr<DBImpl> db(new DBImpl());
    db->open(path);
    return db.release();
}

}  // namespace stuff
