#ifndef FSUTILS_HH
#define FSUTILS_HH

#include <string>

namespace stuff {

bool isdir(const std::string& path);

bool mkdir_p(const std::string& path);

}  // namespace stuff

#endif /* FSUTILS_HH */
