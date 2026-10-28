#pragma once

#include <string>

#include "domain/browser_environment/url_value.h"

namespace ruyijs::domain::browser_environment {

/// Outcome of donor Navigator::CheckProtocolHandlerAllowed for content callers
/// with the default network.protocol-handler prefs. Pure; no engine types.
enum class ProtocolHandlerCheck { Allowed, MissingPlaceholder, DeniedHandler, DeniedScheme };

/// Mirrors dom/base/Navigator.cpp kSafeSchemes (HTML custom-handlers list).
inline bool IsSafeProtocolScheme(const std::string& scheme) {
    static const char* const kSafeSchemes[] = {
        "bitcoin", "ftp", "ftps", "geo", "im", "irc", "ircs", "magnet", "mailto",
        "matrix", "mms", "news", "nntp", "openpgp4fpr", "sftp", "sip", "sms",
        "smsto", "ssh", "tel", "urn", "webcal", "wtai", "xmpp"};
    for (const char* safe : kSafeSchemes) {
        if (scheme == safe) return true;
    }
    return false;
}

/// |handler| is the handler URL already resolved against |document|. The order
/// of checks is the donor's: "%s", http(s) schemes, same host/port and scheme,
/// then the lower-cased scheme (web+[a-z]+ or a safe scheme).
inline ProtocolHandlerCheck CheckProtocolHandler(const std::string& scheme,
                                                 const UrlValue& handler,
                                                 const UrlValue& document) {
    if (handler.href().find("%s") == std::string::npos) {
        return ProtocolHandlerCheck::MissingPlaceholder;
    }
    auto web = [](const std::string& protocol) {
        return protocol == "http:" || protocol == "https:";
    };
    if (!web(document.protocol()) || !web(handler.protocol())) {
        return ProtocolHandlerCheck::DeniedHandler;
    }
    if (handler.host() != document.host() || handler.protocol() != document.protocol()) {
        return ProtocolHandlerCheck::DeniedHandler;
    }
    std::string lower;
    for (char c : scheme) lower += (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c;
    if (lower.rfind("web+", 0) == 0) {
        if (lower.size() == 4) return ProtocolHandlerCheck::DeniedScheme;
        for (size_t i = 4; i < lower.size(); ++i) {
            if (lower[i] < 'a' || lower[i] > 'z') return ProtocolHandlerCheck::DeniedScheme;
        }
        return ProtocolHandlerCheck::Allowed;
    }
    return IsSafeProtocolScheme(lower) ? ProtocolHandlerCheck::Allowed
                                       : ProtocolHandlerCheck::DeniedScheme;
}

}  // namespace ruyijs::domain::browser_environment
