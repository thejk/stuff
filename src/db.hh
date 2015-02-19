#ifndef DB_HH
#define DB_HH

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace stuff {

class DB {
public:
    virtual ~DB() {}

    enum class Type {
        STRING,
        BOOL,
        DOUBLE,
        INT32,
        INT64,
        RAW
    };

    class Constraint {
    public:
        Constraint(Type type)
            : Constraint(type, false, false, false) {
        }

        Type type() const {
            return type_;
        }

        bool primary_key() const {
            return primary_;
        }

        bool unique() const {
            return primary_ || unique_;
        }

        bool not_null() const {
            return primary_ || not_null_;
        }

    protected:
        Constraint(Type type, bool primary, bool unique, bool not_null)
            : type_(type), primary_(primary), unique_(unique),
              not_null_(not_null) {
        }

    private:
        Type type_;
        bool primary_;
        bool unique_;
        bool not_null_;
    };

    class PrimaryKey : public Constraint {
    public:
        explicit PrimaryKey(Type type)
            : Constraint(type, true, true, true) {
        }
    };

    class Unique : public Constraint {
    public:
        explicit Unique(Type type)
            : Constraint(type, false, true, false) {
        }
    };

    class NotNull : public Constraint {
    public:
        explicit NotNull(Type type)
            : Constraint(type, false, false, true) {
        }
    };

    typedef std::vector<std::pair<std::string,Constraint>> Declaration;

    class Editor {
    public:
        virtual ~Editor() {}

        // Set the column matching name to value
        virtual void set(const std::string& name, const std::string& value) = 0;
        virtual void set(const std::string& name, const char* value) {
            set(name, std::string(value));
        }
        virtual void set(const std::string& name, bool value) = 0;
        virtual void set(const std::string& name, double value) = 0;
        virtual void set(const std::string& name, int32_t value) = 0;
        virtual void set(const std::string& name, int64_t value) = 0;
        virtual void set_null(const std::string& name) = 0;
        virtual void set(const std::string& name, const void* data,
                         size_t size) = 0;

        // Return true if the insert/update succeeded, false in case of error
        // After calling commit, release the object, it's no longer usable
        virtual bool commit() = 0;

        // Return the latest inserted rowid
        virtual int64_t last_insert_rowid() = 0;

    protected:
        Editor() {}

    private:
        Editor(const Editor&) = delete;
        Editor& operator=(const Editor&) = delete;
    };

    class Snapshot {
    public:
        virtual ~Snapshot() {}

        // Get the value for the column matching name, returns false if no
        // column matching that name exists or if the type doesn't match
        virtual bool get(const std::string& name, std::string* value) = 0;
        virtual bool get(const std::string& name, bool* value) = 0;
        virtual bool get(const std::string& name, double* value) = 0;
        virtual bool get(const std::string& name, int32_t* value) = 0;
        virtual bool get(const std::string& name, int64_t* value) = 0;
        virtual bool get(const std::string& name,
                         std::vector<uint8_t>* value) = 0;
        virtual bool is_null(const std::string& name, bool* value) = 0;

        // Set the column to value, returns false if no column with that index
        // exists or if the type doesn't match
        virtual bool get(uint32_t column, std::string* value) = 0;
        virtual bool get(uint32_t column, bool* value) = 0;
        virtual bool get(uint32_t column, double* value) = 0;
        virtual bool get(uint32_t column, int32_t* value) = 0;
        virtual bool get(uint32_t column, int64_t* value) = 0;
        virtual bool get(uint32_t column, std::vector<uint8_t>* value) = 0;
        virtual bool is_null(uint32_t column, bool* value) = 0;

        // Go to the next row in snapshot, returns false if this was the last
        // or an error occurred
        virtual bool next() = 0;

        // Returns true if the snapshot had an error
        virtual bool bad() = 0;

    protected:
        Snapshot() {}

    private:
        Snapshot(const Snapshot&) = delete;
        Snapshot& operator=(const Snapshot&) = delete;
    };

    class Condition;

    class Value {
    public:
        explicit Value(const std::string& value);
        Value(int32_t value);
        Value(int64_t value);
        Value(bool value);
        Value(double value);
        Value(std::nullptr_t);

        Type type() const {
            return type_;
        }

        const std::string& string() const;

        int32_t i32() const;
        int64_t i64() const;
        bool b() const;
        double d() const;

    private:
        friend class Condition;
        Value() {}

        Type type_;
        std::string string_;
        union {
            int32_t i32;
            int64_t i64;
            bool b;
            double d;
        } data_;
    };

    class Column {
    public:
        explicit Column(const std::string& name)
            : name_(name) {
        }

        const std::string& name() const {
            return name_;
        }

    private:
        friend class Condition;
        Column() {}

        std::string name_;
    };

    class Condition {
    public:
        enum BinaryBooleanOperator {
            AND,
            OR
        };
        enum UnaryBooleanOperator {
            NOT
        };
        enum BinaryOperator {
            EQUAL,
            NOT_EQUAL,
            GREATER_THAN,
            LESS_THAN,
            GREATER_EQUAL,
            LESS_EQUAL,
        };
        enum UnaryOperator {
            NEGATIVE,
            IS_NULL,
        };
        enum Mode {
            NOOP,
            BOOL_BINARY,
            BOOL_UNARY,
            COMP_BINARY,
            COMP_UNARY
        };

        Condition()
            : mode_(NOOP) {
        }
        Condition(const Condition& c1, BinaryBooleanOperator op,
                  const Condition& c2)
            : mode_(BOOL_BINARY), c1_(new Condition(c1)),
              c2_(new Condition(c2)) {
            assert(!c1_->empty() && !c2_->empty());
            op_.bool_binary = op;
        }
        Condition(UnaryBooleanOperator op, const Condition& c)
            : mode_(BOOL_UNARY), c1_(new Condition(c)) {
            assert(!c1_->empty());
            op_.bool_unary = op;
        }
        Condition(const Column& c, BinaryOperator op, const Value& v)
            : mode_(COMP_BINARY), c_(c), v_(v) {
            op_.comp_binary = op;
        }
        Condition(UnaryOperator op, const Column& c)
            : mode_(COMP_UNARY), c_(c) {
            op_.comp_unary = op;
        }

        Mode mode() const {
            return mode_;
        }

        bool empty() const {
            return mode_ == NOOP;
        }

        const Condition& c1() const {
            assert(mode_ == BOOL_BINARY || mode_ == BOOL_UNARY);
            return *c1_;
        }

        BinaryBooleanOperator bool_binary_op() const {
            assert(mode_ == BOOL_BINARY);
            return op_.bool_binary;
        }

        UnaryBooleanOperator bool_unary_op() const {
            assert(mode_ == BOOL_UNARY);
            return op_.bool_unary;
        }

        const Condition& c2() const {
            assert(mode_ == BOOL_BINARY);
            return *c2_;
        }

        BinaryOperator binary_op() const {
            assert(mode_ == COMP_BINARY);
            return op_.comp_binary;
        }

        UnaryOperator unary_op() const {
            assert(mode_ == COMP_UNARY);
            return op_.comp_unary;
        }

        const Column& column() const {
            assert(mode_ == COMP_BINARY || mode_ == COMP_UNARY);
            return c_;
        }

        const Value& value() const {
            assert(mode_ == COMP_BINARY);
            return v_;
        }

    private:
        const Mode mode_;
        const std::shared_ptr<Condition> c1_, c2_;
        const Column c_;
        const Value v_;
        union {
            BinaryBooleanOperator bool_binary;
            UnaryBooleanOperator bool_unary;
            BinaryOperator comp_binary;
            UnaryOperator comp_unary;
        } op_;
    };

    class OrderBy {
    public:
        OrderBy(const std::string& name, bool ascending = true)
            : name_(name), ascending_(ascending) {
        }
        OrderBy(const Column& column, bool ascending = true)
            : name_(column.name()), ascending_(ascending) {
        }

        const std::string& name() const {
            return name_;
        }

        bool ascending() const {
            return ascending_;
        }

    private:
        std::string name_;
        bool ascending_;
    };

    // Create a table with the given declarations.
    // If a table with that name already exists nothing happens.
    // Returns false in case of error
    virtual bool insert_table(const std::string& table,
                              const Declaration& declaration) = 0;
    // Returns false in case of error. The table not existing is not an error.
    virtual bool remove_table(const std::string& table) = 0;
    // Create an editor for inserting an row in the table.
    // Always succeeds and doesn't do anything until commit is called on the
    // Editor.
    virtual std::shared_ptr<Editor> insert(const std::string& table) = 0;
    // Create an editor for updating an row in the table.
    // Always succeeds and doesn't do anything until commit is called on the
    // Editor.
    virtual std::shared_ptr<Editor> update(const std::string& table,
                                           const Condition& condition =
                                           Condition()) = 0;
    // Return a snapshot for rows in table matching condition.
    // Check bad() in snapshot for errors
    virtual std::shared_ptr<Snapshot> select(
            const std::string& table, const Condition& condition = Condition(),
            const std::vector<OrderBy>& order_by = std::vector<OrderBy>()) = 0;
    // Remove all rows matching condition in table. Returns number of rows
    // removed or -1 in case of error
    virtual int64_t remove(const std::string& table,
                           const Condition& condition = Condition()) = 0;

    virtual bool start_transaction() = 0;
    virtual bool commit_transaction() = 0;
    virtual bool rollback_transaction() = 0;

    // Returns true if a fatal database error has occurred
    virtual bool bad() = 0;

    // Returns a description of the last error returned by an earlier method
    virtual std::string last_error() = 0;

protected:
    DB() {}

private:
    DB(const DB&) = delete;
    DB& operator=(const DB&) = delete;
};

inline DB::Condition operator&&(const DB::Condition& c1,
                                const DB::Condition& c2) {
    return DB::Condition(c1, DB::Condition::AND, c2);
}
inline DB::Condition operator||(const DB::Condition& c1,
                                const DB::Condition& c2) {
    return DB::Condition(c1, DB::Condition::OR, c2);
}
inline DB::Condition operator!(const DB::Condition& c) {
    return DB::Condition(DB::Condition::NOT, c);
}
inline DB::Condition operator==(const DB::Column& c, const DB::Value& v) {
    return DB::Condition(c, DB::Condition::EQUAL, v);
}
inline DB::Condition operator!=(const DB::Column& c, const DB::Value& v) {
    return DB::Condition(c, DB::Condition::NOT_EQUAL, v);
}
inline DB::Condition operator<(const DB::Column& c, const DB::Value& v) {
    return DB::Condition(c, DB::Condition::LESS_THAN, v);
}
inline DB::Condition operator>(const DB::Column& c, const DB::Value& v) {
    return DB::Condition(c, DB::Condition::GREATER_THAN, v);
}
inline DB::Condition operator<=(const DB::Column& c, const DB::Value& v) {
    return DB::Condition(c, DB::Condition::LESS_EQUAL, v);
}
inline DB::Condition operator>=(const DB::Column& c, const DB::Value& v) {
    return DB::Condition(c, DB::Condition::GREATER_EQUAL, v);
}
inline DB::Condition operator-(const DB::Column& c) {
    return DB::Condition(DB::Condition::NEGATIVE, c);
}
inline DB::Condition is_null(const DB::Column& c) {
    return DB::Condition(DB::Condition::IS_NULL, c);
}

}  // namespace stuff

#endif /* DB_HH */
