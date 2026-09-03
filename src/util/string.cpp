#include "util/string.hpp"

namespace pi {

bool startsWith(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

bool endsWith(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}

bool contains(std::string_view s, std::string_view needle) {
    return s.find(needle) != std::string_view::npos;
}

} // namespace pi
