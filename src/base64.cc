#include "common.hh"

#include "base64.hh"

namespace stuff {

namespace {

int8_t value(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return 26 + c - 'a';
    }
    if (c >= '0' && c <= '9') {
        return 52 + c - '0';
    }
    if (c == '+' || c == '-') {
        return 62;
    }
    if (c == '/' || c == '_') {
        return 63;
    }
    return -1;
}

bool decode_one(const int8_t buf[4], std::string* output) {
    if (buf[1] == -1 || buf[0] == -1) {
        // There is no reason for the first or the second char to be
        // padding
        return false;
    }
    output->push_back((buf[0] << 2) | (buf[1] >> 4));
    if (buf[2] == -1) {
        return buf[3] == -1;  // No padding between non-padding
    }
    output->push_back(((buf[1] & 0xf) << 4) | (buf[2] >> 2));
    if (buf[3] == -1) return true;
    output->push_back(((buf[2] & 0x3) << 6) | buf[3]);
    return true;
}

}  // namespace

bool Base64::decode(const std::string& input, std::string* output) {
    int8_t buf[4];
    size_t fill = 0;
    bool done = false;
    output->clear();
    for (auto it = input.begin(); it != input.end(); ++it) {
        buf[fill] = value(*it);
        if (buf[fill] != -1 || *it == '=') {
            if (done) return false;  // crap after padding
            if (++fill == 4) {
                if (!decode_one(buf, output)) return false;
                done = buf[3] == -1;
                fill = 0;
            }
        }
    }
    if (fill != 0) {
        while (fill < 4) buf[fill] = -1;
        if (!decode_one(buf, output)) return false;
    }
    return true;
}

}  // namespace stuff
