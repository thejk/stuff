#ifndef BASE64_HH
#define BASE64_HH

#include <string>

namespace stuff {

class Base64 {
public:
    static bool decode(const std::string& input, std::string* output);

private:
    Base64() = delete;
    ~Base64() = delete;
    Base64(Base64&) = delete;
    Base64& operator=(Base64&) = delete;
};

}  // namespace stuff

#endif /* BASE64_HH */
