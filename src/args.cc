#include "common.hh"

#include "args.hh"

namespace stuff {

// static
bool Args::parse(const std::string& input, std::vector<std::string>* output,
                 bool nice) {
    size_t last = 0, i = 0;
    std::string arg;
    output->clear();
    while (i < input.size() && input[i] == ' ') ++i;
    if (i == input.size()) return true;
    while (i < input.size()) {
        if (input[i] == ' ') {
            arg.append(input.substr(last, i - last));
            output->push_back(arg);
            arg.clear();
            ++i;
            while (i < input.size() && input[i] == ' ') ++i;
            if (i == input.size()) return true;
            last = i;
        } else if (input[i] == '\'' || input[i] == '"') {
            const char closing = input[i];
            std::string text;
            size_t j = i + 1;
            size_t last_j = j;
            while (j < input.size()) {
                if (input[j] == closing) {
                    break;
                } else if (input[j] == '\\') {
                    text.append(input.substr(last_j, j - last_j));
                    if (j == input.size()) {
                        if (!nice) return false;
                        break;
                    }
                    text.push_back(input[++j]);
                    last_j = ++j;
                } else {
                    ++j;
                }
            }
            if (j < input.size()) {
                arg.append(input.substr(last, i - last));
                arg.append(text);
                arg.append(input.substr(last_j, j - last_j));
                last = i = j + 1;
            } else {
                // No closing char
                if (!nice) return false;
                ++i;
            }
        } else {
            ++i;
        }
    }
    arg.append(input.substr(last, i - last));
    output->push_back(arg);
    return true;
}

}  // namespace stuff
