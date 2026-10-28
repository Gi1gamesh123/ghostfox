#pragma once
#include "domain/resource_network/header_rules.h"
#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ruyijs::domain::resource_network {
using ResponseHeaderPairs = std::vector<std::pair<std::string, std::string>>;

inline bool forbidden_response_header(std::string_view name) {
    const auto lower = lower_header_name(name);
    return lower == "set-cookie" || lower == "set-cookie2";
}

inline bool cors_safelisted_response_header(std::string_view name) {
    const auto lower = lower_header_name(name);
    return lower == "cache-control" || lower == "content-language" ||
        lower == "content-length" || lower == "content-type" ||
        lower == "expires" || lower == "last-modified" || lower == "pragma";
}

inline void AppendExposeNames(std::string_view value,
                              std::vector<std::string>* names, bool* wildcard) {
    while (!value.empty()) {
        const size_t comma = value.find(',');
        std::string_view token = value.substr(0, comma);
        token = trim_header_value(token);
        if (!token.empty()) {
            auto lower = lower_header_name(token);
            if (lower == "*") *wildcard = true;
            names->push_back(std::move(lower));
        }
        if (comma == std::string_view::npos) break;
        value.remove_prefix(comma + 1);
    }
}

inline ResponseHeaderPairs FilterResponseHeaders(
        const ResponseHeaderPairs& input, bool crossOrigin,
        std::string_view mode, bool credentialsInclude) {
    std::vector<std::string> exposed;
    bool wildcard = false;
    if (crossOrigin && mode == "cors") {
        for (const auto& field : input)
            if (lower_header_name(field.first) == "access-control-expose-headers")
                AppendExposeNames(field.second, &exposed, &wildcard);
        wildcard = wildcard && !credentialsInclude;
    }
    ResponseHeaderPairs visible;
    for (const auto& field : input) {
        const auto name = lower_header_name(field.first);
        if (forbidden_response_header(name)) continue;
        if (crossOrigin && mode == "cors" && !cors_safelisted_response_header(name) &&
            !wildcard && std::find(exposed.begin(), exposed.end(), name) == exposed.end())
            continue;
        visible.push_back(field);
    }
    return visible;
}
}
