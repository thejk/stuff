#ifndef JSON_HH
#define JSON_HH

#include <memory>
#include <string>

namespace stuff {

class JsonArray;

class JsonObject {
public:
    virtual ~JsonObject() {}
    virtual void put(const std::string& name, const std::string& value) = 0;
    virtual void put(const std::string& name, double value) = 0;
    virtual void put(const std::string& name, int64_t value) = 0;
    virtual void put(const std::string& name, bool value) = 0;
    virtual void put(const std::string& name, std::nullptr_t value) = 0;
    virtual void put(const std::string& name,
                     std::shared_ptr<JsonObject> obj) = 0;
    virtual void put(const std::string& name,
                     std::shared_ptr<JsonArray> arr) = 0;

    virtual bool contains(const std::string& name) const = 0;
    virtual bool is_null(const std::string& name) const = 0;
    virtual const std::string& get(const std::string& name,
                                   const std::string& fallback
                                   = std::string()) const = 0;
    virtual double get(const std::string& name,
                       double fallback = 0.0) const = 0;
    virtual int64_t get(const std::string& name,
                        int64_t fallback = 0) const = 0;
    virtual bool get(const std::string& name, bool fallback) const = 0;
    virtual bool get(const std::string& name,
                     std::shared_ptr<JsonObject>* obj) const = 0;
    virtual bool get(const std::string& name,
                     std::shared_ptr<JsonArray>* arr) const = 0;

    virtual std::string str() const = 0;

    static std::shared_ptr<JsonObject> create();

protected:
    JsonObject() {}
    JsonObject(const JsonObject&) = delete;
    JsonObject& operator=(const JsonObject&) = delete;
};

class JsonArray {
public:
    virtual ~JsonArray() {}

    virtual size_t size() const = 0;
    virtual void resize(size_t size) = 0;

    virtual void put(size_t index, const std::string& value) = 0;
    virtual void put(size_t index, double value) = 0;
    virtual void put(size_t index, int64_t value) = 0;
    virtual void put(size_t index, bool value) = 0;
    virtual void put(size_t index, std::nullptr_t value) = 0;
    virtual void put(size_t index,
                     std::shared_ptr<JsonObject> obj) = 0;
    virtual void put(size_t index,
                     std::shared_ptr<JsonArray> arr) = 0;

    void add(const std::string& value);
    void add(double value);
    void add(int64_t value);
    void add(bool value);
    void add(std::nullptr_t value);
    void add(std::shared_ptr<JsonObject> obj);
    void add(std::shared_ptr<JsonArray> arr);

    virtual bool is_null(size_t index) const = 0;
    virtual const std::string& get(size_t index,
                                   const std::string& fallback
                                   = std::string()) const = 0;
    virtual double get(size_t index,
                       double fallback = 0.0) const = 0;
    virtual int64_t get(size_t index,
                        int64_t fallback = 0) const = 0;
    virtual bool get(size_t index, bool fallback) const = 0;
    virtual bool get(size_t index,
                     std::shared_ptr<JsonObject>* obj) const = 0;
    virtual bool get(size_t index,
                     std::shared_ptr<JsonArray>* arr) const = 0;

    virtual std::string str() const = 0;

    static std::shared_ptr<JsonArray> create();

protected:
    JsonArray() {}
    JsonArray(const JsonArray&) = delete;
    JsonArray& operator=(const JsonArray&) = delete;
};

}  // namespace stuff

#endif /* JSON_HH */
