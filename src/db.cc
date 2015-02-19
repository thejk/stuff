#include "common.hh"

#include "db.hh"

namespace stuff {

DB::Value::Value(const std::string& value)
    : type_(DB::Type::STRING), string_(value) {
}

DB::Value::Value(int32_t value)
    : type_(DB::Type::INT32) {
    data_.i32 = value;
}

DB::Value::Value(int64_t value)
    : type_(DB::Type::INT64) {
    data_.i64 = value;
}

DB::Value::Value(bool value)
    : type_(DB::Type::BOOL) {
    data_.b = value;
}

DB::Value::Value(double value)
    : type_(DB::Type::DOUBLE) {
    data_.d = value;
}

DB::Value::Value(std::nullptr_t)
    : type_(DB::Type::RAW) {
}

const std::string& DB::Value::string() const {
    assert(type_ == DB::Type::STRING);
    return string_;
}

int32_t DB::Value::i32() const {
    assert(type_ == DB::Type::INT32);
    return data_.i32;
}

int64_t DB::Value::i64() const {
    assert(type_ == DB::Type::INT64);
    return data_.i64;
}

bool DB::Value::b() const {
    assert(type_ == DB::Type::BOOL);
    return data_.b;
}

double DB::Value::d() const {
    assert(type_ == DB::Type::DOUBLE);
    return data_.d;
}

}  // namespace stuff
