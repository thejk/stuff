#include "common.hh"

#include <sys/stat.h>
#include <sys/types.h>
#include <cerrno>
#include <cstring>
#include <libgen.h>

#include "fsutils.hh"

namespace stuff {

bool isdir(const std::string& path) {
    struct stat buf;
    if (stat(path.c_str(), &buf)) {
        return false;
    }
    return S_ISDIR(buf.st_mode);
}

bool mkdir_p(const std::string& path) {
    if (mkdir(path.c_str(), 0777)) {
        if (errno == EEXIST) return isdir(path);
        char* dir = dirname(const_cast<char*>(path.c_str()));
        if (strcmp(dir, ".") == 0 || strcmp(dir, "/") == 0) return false;
        if (!mkdir_p(dir)) return false;
        if (mkdir(path.c_str(), 0777)) {
            return errno == EEXIST && isdir(path);
        }
    }
    return true;
}

}  // namespace
