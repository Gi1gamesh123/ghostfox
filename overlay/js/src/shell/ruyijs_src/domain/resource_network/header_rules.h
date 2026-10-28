#pragma once
#include <string>
#include <string_view>

namespace ruyijs::domain::resource_network {
inline bool valid_header_name(std::string_view name) noexcept {
    if (name.empty()) return false;
    for (unsigned char c : name) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9')) continue;
        switch (c) {
            case '!': case '#': case '$': case '%': case '&': case '\'':
            case '*': case '+': case '-': case '.': case '^': case '_':
            case '`': case '|': case '~': continue;
            default: return false;
        }
    }
    return true;
}
inline std::string lower_header_name(std::string_view name) {
    std::string result(name);
    for (char& c : result) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return result;
}
inline std::string_view trim_header_value(std::string_view value) noexcept {
    auto whitespace = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!value.empty() && whitespace(value.front())) value.remove_prefix(1);
    while (!value.empty() && whitespace(value.back())) value.remove_suffix(1);
    return value;
}
inline bool valid_header_value(std::string_view value) noexcept {
    for (char c : value) if (c == '\0' || c == '\r' || c == '\n') return false;
    return true;
}
}
