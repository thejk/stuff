#include "common.hh"

#include <sstream>
#include <unordered_map>
#include <vector>

#include "json.hh"

namespace stuff {

namespace {

enum class JsonType {
    STRING,
    OBJECT,
    ARRAY,
    INT64,
    DOUBLE,
    BOOL,
};

struct JsonValue {
    const JsonType type;
    JsonValue(JsonType type)
        : type(type) {
    }
};

struct StringJsonValue : public JsonValue {
    StringJsonValue(const std::string& str)
        : JsonValue(JsonType::STRING), str(str) {
    }
    std::string str;
};

struct ObjectJsonValue : public JsonValue {
    ObjectJsonValue(std::shared_ptr<JsonObject> obj)
        : JsonValue(JsonType::OBJECT), obj(obj) {
    }
    std::shared_ptr<JsonObject> obj;
};

struct ArrayJsonValue : public JsonValue {
    ArrayJsonValue(std::shared_ptr<JsonArray> array)
        : JsonValue(JsonType::ARRAY), array(array) {
    }
    std::shared_ptr<JsonArray> array;
};

struct BasicJsonValue : public JsonValue {
    BasicJsonValue(double d)
        : JsonValue(JsonType::DOUBLE), data({ .d = d }) {
    }
    BasicJsonValue(int64_t i)
        : JsonValue(JsonType::INT64), data({ .i = i }) {
    }
    BasicJsonValue(bool b)
        : JsonValue(JsonType::BOOL), data({ .b = b }) {
    }
    union {
        double d;
        int64_t i;
        bool b;
    } data;
};

class JsonObjectImpl;
class JsonArrayImpl;

std::ostream& write(std::ostream& os, const JsonObjectImpl* obj);
std::ostream& write(std::ostream& os, const JsonArrayImpl* array);
std::ostream& write(std::ostream& os, const JsonValue* value);

class JsonObjectImpl : public JsonObject {
public:
    JsonObjectImpl() {
    }

    void put(const std::string& name, const std::string& value) override {
        put(name, new StringJsonValue(value));
    }
    void put(const std::string& name, double value) override {
        put(name, new BasicJsonValue(value));
    }
    void put(const std::string& name, int64_t value) override {
        put(name, new BasicJsonValue(value));
    }
    void put(const std::string& name, bool value) override {
        put(name, new BasicJsonValue(value));
    }
    void put(const std::string& name, std::nullptr_t value) override {
        put(name, value);
    }
    void put(const std::string& name,
             std::shared_ptr<JsonObject> obj) override {
        if (obj) {
            put(name, new ObjectJsonValue(obj));
        } else {
            put(name, nullptr);
        }
    }
    void put(const std::string& name,
             std::shared_ptr<JsonArray> array) override {
        if (array) {
            put(name, new ArrayJsonValue(array));
        } else {
            put(name, nullptr);
        }
    }

    bool contains(const std::string& name) const override {
        return data_.find(name) != data_.end();
    }
    bool is_null(const std::string& name) const override {
        auto it = data_.find(name);
        return it != data_.end() && !it->second;
    }
    const std::string& get(const std::string& name,
                           const std::string& fallback) const override {
        auto it = data_.find(name);
        if (it == data_.end() || !it->second) return fallback;
        return it->second->type == JsonType::STRING ?
            static_cast<StringJsonValue*>(it->second.get())->str : fallback;
    }
    double get(const std::string& name, double fallback) const override {
        auto it = data_.find(name);
        if (it == data_.end() || !it->second) return fallback;
        return it->second->type == JsonType::DOUBLE ?
            static_cast<BasicJsonValue*>(it->second.get())->data.d : fallback;
    }
    int64_t get(const std::string& name, int64_t fallback) const override {
        auto it = data_.find(name);
        if (it == data_.end() || !it->second) return fallback;
        return it->second->type == JsonType::INT64 ?
            static_cast<BasicJsonValue*>(it->second.get())->data.i : fallback;
    }
    bool get(const std::string& name, bool fallback) const override {
        auto it = data_.find(name);
        if (it == data_.end() || !it->second) return fallback;
        return it->second->type == JsonType::BOOL ?
            static_cast<BasicJsonValue*>(it->second.get())->data.b : fallback;
    }
    bool get(const std::string& name,
             std::shared_ptr<JsonObject>* obj) const override {
        auto it = data_.find(name);
        obj->reset();
        if (it == data_.end() || !it->second) return false;
        if (it->second->type != JsonType::OBJECT) return false;
        *obj = static_cast<ObjectJsonValue*>(it->second.get())->obj;
        return true;
    }
    bool get(const std::string& name,
             std::shared_ptr<JsonArray>* array) const override {
        auto it = data_.find(name);
        array->reset();
        if (it == data_.end() || !it->second) return false;
        if (it->second->type != JsonType::ARRAY) return false;
        *array = static_cast<ArrayJsonValue*>(it->second.get())->array;
        return true;
    }

    std::string str() const {
        std::ostringstream ss;
        write(ss, this);
        return ss.str();
    }

    void put(const std::string& name, std::unique_ptr<JsonValue> ptr) {
        data_[name].swap(ptr);
    }

    std::unordered_map<std::string, std::unique_ptr<JsonValue>> data_;
};

class JsonArrayImpl : public JsonArray {
public:
    JsonArrayImpl() {
    }

    void put(size_t index, const std::string& value) override {
        put(index, new StringJsonValue(value));
    }
    void put(size_t index, double value) override {
        put(index, new BasicJsonValue(value));
    }
    void put(size_t index, int64_t value) override {
        put(index, new BasicJsonValue(value));
    }
    void put(size_t index, bool value) override {
        put(index, new BasicJsonValue(value));
    }
    void put(size_t index, std::nullptr_t value) override {
        put(index, value);
    }
    void put(size_t index,
             std::shared_ptr<JsonObject> obj) override {
        if (obj) {
            put(index, new ObjectJsonValue(obj));
        } else {
            put(index, nullptr);
        }
    }
    void put(size_t index,
             std::shared_ptr<JsonArray> array) override {
        if (array) {
            put(index, new ArrayJsonValue(array));
        } else {
            put(index, nullptr);
        }
    }

    bool is_null(size_t index) const override {
        return index < data_.size() && !data_[index];
    }
    const std::string& get(size_t index,
                           const std::string& fallback) const override {
        if (index >= data_.size() || !data_[index]) return fallback;
        return data_[index]->type == JsonType::STRING ?
            static_cast<StringJsonValue*>(data_[index].get())->str : fallback;
    }
    double get(size_t index, double fallback) const override {
        if (index >= data_.size() || !data_[index]) return fallback;
        return data_[index]->type == JsonType::DOUBLE ?
            static_cast<BasicJsonValue*>(data_[index].get())->data.d : fallback;
    }
    int64_t get(size_t index, int64_t fallback) const override {
        if (index >= data_.size() || !data_[index]) return fallback;
        return data_[index]->type == JsonType::INT64 ?
            static_cast<BasicJsonValue*>(data_[index].get())->data.i : fallback;
    }
    bool get(size_t index, bool fallback) const override {
        if (index >= data_.size() || !data_[index]) return fallback;
        return data_[index]->type == JsonType::BOOL ?
            static_cast<BasicJsonValue*>(data_[index].get())->data.b : fallback;
    }
    bool get(size_t index,
             std::shared_ptr<JsonObject>* obj) const override {
        obj->reset();
        if (index >= data_.size() || !data_[index]) return false;
        if (data_[index]->type != JsonType::OBJECT) return false;
        *obj = static_cast<ObjectJsonValue*>(data_[index].get())->obj;
        return true;
    }
    bool get(size_t index,
             std::shared_ptr<JsonArray>* array) const override {
        array->reset();
        if (index >= data_.size() || !data_[index]) return false;
        if (data_[index]->type != JsonType::ARRAY) return false;
        *array = static_cast<ArrayJsonValue*>(data_[index].get())->array;
        return true;
    }

    std::string str() const {
        std::ostringstream ss;
        write(ss, this);
        return ss.str();
    }

    size_t size() const override {
        return data_.size();
    }

    void resize(size_t size) override {
        data_.resize(size);
    }

    void put(size_t index, std::unique_ptr<JsonValue> ptr) {
        while (index >= data_.size()) {
            data_.emplace_back(nullptr);
        }
        data_[index].swap(ptr);
    }

    std::vector<std::unique_ptr<JsonValue>> data_;
};

std::ostream& quoted(std::ostream& os, const std::string& str) {
    os << '"';
    size_t last = 0;
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '"' || str[i] == '\\') {
            os << str.substr(last, i - last);
            os << '\\';
            last = i;
        }
    }
    os << str.substr(last);
    return os << '"';
}

std::ostream& write(std::ostream& os, const JsonObjectImpl* obj) {
    os << '{';
    bool first = true;
    for (const auto& pair : obj->data_) {
        if (!first) {
            os << ',';
        } else {
            first = false;
        }
        quoted(os, pair.first);
        os << ':';
        write(os, pair.second.get());
    }
    return os << '}';
}

std::ostream& write(std::ostream& os, const JsonArrayImpl* array) {
    os << '[';
    bool first = true;
    for (const auto& value : array->data_) {
        if (!first) {
            os << ',';
        } else {
            first = false;
        }
        write(os, value.get());
    }
    return os << ']';
}

std::ostream& write(std::ostream& os, const JsonValue* value) {
    if (value) {
        switch (value->type) {
        case JsonType::STRING:
            return quoted(os, static_cast<const StringJsonValue*>(value)->str);
        case JsonType::DOUBLE:
            return os << static_cast<const BasicJsonValue*>(value)->data.d;
        case JsonType::INT64:
            return os << static_cast<const BasicJsonValue*>(value)->data.i;
        case JsonType::BOOL:
            return os << (static_cast<const BasicJsonValue*>(value)->data.b ?
                          "true" : "false");
        case JsonType::OBJECT:
            return write(
                    os, static_cast<const JsonObjectImpl*>(
                            static_cast<const ObjectJsonValue*>(value)
                            ->obj.get()));
        case JsonType::ARRAY:
            return write(
                    os, static_cast<const JsonArrayImpl*>(
                            static_cast<const ArrayJsonValue*>(value)
                            ->array.get()));
        }
    }
    return os << "null";
}

}  // namespace

// static
std::shared_ptr<JsonObject> JsonObject::create() {
    return std::make_shared<JsonObjectImpl>();
}

void JsonArray::add(const std::string& value) {
    put(size(), value);
}

void JsonArray::add(double value) {
    put(size(), value);
}

void JsonArray::add(int64_t value) {
    put(size(), value);
}

void JsonArray::add(bool value) {
    put(size(), value);
}

void JsonArray::add(std::nullptr_t value) {
    put(size(), value);
}

void JsonArray::add(std::shared_ptr<JsonObject> obj) {
    put(size(), obj);
}

void JsonArray::add(std::shared_ptr<JsonArray> arr) {
    put(size(), arr);
}

// static
std::shared_ptr<JsonArray> JsonArray::create() {
    return std::make_shared<JsonArrayImpl>();
}

}  // namespace stuff
