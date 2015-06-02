#ifndef ARGS_HH
#define ARGS_HH

#include <string>
#include <vector>

namespace stuff {

class Args {
public:
    static bool parse(const std::string& input,
                      std::vector<std::string>* output,
                      bool nice = true);

private:
    Args() = delete;
    ~Args() = delete;
};

}  // namespace stuff

#endif /* ARGS_HH */
