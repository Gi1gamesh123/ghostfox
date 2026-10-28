#pragma once
#include "domain/resource_network/header_rules.h"
#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ruyijs::domain::resource_network {
enum class RedirectMode { Follow, Error, Manual };
inline constexpr unsigned kRedirectLimit = 20;

inline bool is_redirect_status(int status) noexcept {
    return status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
}
inline bool redirect_changes_to_get(int status, std::string_view method) noexcept {
    return ((status == 301 || status == 302) && method == "POST") ||
           (status == 303 && method != "GET" && method != "HEAD");
}
inline void redirect_headers(std::vector<std::pair<std::string, std::string>>& headers,
                             bool discard_body, bool cross_origin) {
    headers.erase(std::remove_if(headers.begin(), headers.end(), [&](const auto& field) {
        const auto name = lower_header_name(field.first);
        if (discard_body && (name == "content-encoding" || name == "content-language" ||
            name == "content-location" || name == "content-type" ||
            name == "content-length" || name == "transfer-encoding")) return true;
        return cross_origin && (name == "authorization" || name == "cookie" ||
                                name == "proxy-authorization");
    }), headers.end());
}
}
