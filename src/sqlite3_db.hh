#ifndef SQLITE3_DB_HH
#define SQLITE3_DB_HH

#include "db.hh"

#include <memory>
#include <string>

namespace stuff {

class SQLite3 {
public:
    static std::unique_ptr<DB> open(const std::string& path);
};

}  // namespace stuff

#endif /* SQLITE3_DB_HH */
