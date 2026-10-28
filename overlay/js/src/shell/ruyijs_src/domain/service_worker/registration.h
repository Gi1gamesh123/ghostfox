#pragma once

#include <string>

#include "domain/browser_environment/url_value.h"

namespace ruyijs::domain::service_worker {

using ::ruyijs::domain::browser_environment::UrlValue;

/// The register() URL checks of donor ServiceWorkerContainer::Register and
/// ServiceWorkerUtils.cpp (ServiceWorkerScopeAndScriptAreValid /
/// ServiceWorkerScopeIsValid / CheckForSlashEscapedCharsInPath), in their
/// order. `kind` is the DOM error to report; `message` is the donor text.
struct RegisterCheck {
    enum Kind { Ok, TypeError, SecurityError } kind = Ok;
    std::string message;
    std::string scope, script;
};

inline std::string WithoutRef(const UrlValue& url) {
    const std::string& href = url.href();
    return href.substr(0, href.find('#'));
}

inline bool SlashEscaped(const std::string& path) {
    std::string lower;
    for (char c : path) lower += (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c;
    return lower.find("%2f") != std::string::npos || lower.find("%5c") != std::string::npos;
}

/// `prefix` is the binding prefix ("ServiceWorkerContainer.register: ") used by
/// the TypeError messages; `document` is the client's (and base) URL.
inline RegisterCheck CheckRegistration(const std::string& prefix, const std::string& script_input,
                                       bool has_scope, const std::string& scope_input,
                                       const std::string& document) {
    RegisterCheck result;
    UrlValue script, scope;
    if (!UrlValue::Resolve(script_input, document, &script)) {
        return {RegisterCheck::TypeError, prefix + script_input + " is not a valid URL."};
    }
    const std::string base = has_scope ? document : script.href();
    const std::string scope_text = has_scope ? scope_input : "./";
    if (!UrlValue::Resolve(scope_text, base, &scope) || !scope.is_valid()) {
        return {RegisterCheck::TypeError,
                prefix + "Invalid scope trying to resolve " + scope_text + " with base URL " + base + "."};
    }
    auto web = [](const UrlValue& url) { return url.protocol() == "http:" || url.protocol() == "https:"; };
    UrlValue client;
    (void)UrlValue::Resolve(document, "", &client);
    if (!web(script)) return {RegisterCheck::TypeError, prefix + "Script URL's scheme is not 'http' or 'https'"};
    if (SlashEscaped(script.pathname())) return {RegisterCheck::TypeError, prefix + "script URL contains %2f or %5c"};
    if (!web(scope)) return {RegisterCheck::TypeError, prefix + "Scope URL's scheme is not 'http' or 'https'"};
    if (SlashEscaped(scope.pathname())) return {RegisterCheck::TypeError, prefix + "scope URL contains %2f or %5c"};
    if (scope.origin() != client.origin()) return {RegisterCheck::SecurityError, prefix + "Non-same-origin scope URL"};
    if (script.origin() != client.origin())
        return {RegisterCheck::SecurityError, prefix + "Script URL is not same-origin with Client"};
    result.scope = WithoutRef(scope);
    result.script = WithoutRef(script);
    return result;
}

}  // namespace ruyijs::domain::service_worker
