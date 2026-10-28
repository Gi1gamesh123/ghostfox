#pragma once

#include "domain/browser_environment/url_idna.h"
#include <string>
#include <vector>
#include "domain/browser_environment/url_query.h"
#include "domain/browser_environment/origin_value.h"

namespace ruyijs::domain::browser_environment {

/// Value Object: a parsed URL.
///
/// This is the product's single URL parser. The browser-environment Location,
/// the web-platform `URL` interface and the runtime request resolver all read
/// the same component model, so a URL never has two interpretations. Parsing
/// covers scheme, userinfo, host, port, path, query and fragment, relative
/// resolution against a base and dot-segment removal; the rules the product
/// deliberately leaves out are recorded in the deviations ledger.
class UrlValue {
public:
    UrlValue() = default;

    explicit UrlValue(const std::string& href) { Parse(href, nullptr, this); }

    // Component accessors
    const std::string& href() const noexcept { return href_; }
    const std::string& protocol() const noexcept { return protocol_; }
    const std::string& username() const noexcept { return username_; }
    const std::string& password() const noexcept { return password_; }
    const std::string& host() const noexcept { return host_; }
    const std::string& hostname() const noexcept { return hostname_; }
    const std::string& port() const noexcept { return port_; }
    const std::string& pathname() const noexcept { return pathname_; }
    const std::string& search() const noexcept { return search_; }
    const std::string& hash() const noexcept { return hash_; }

    /// Origin = protocol + "//" + host, or "null" without an authority.
    OriginValue origin_value() const {
        if (protocol_ == "blob:") {
            UrlValue inner;
            if (!Parse(pathname_, nullptr, &inner) ||
                (inner.protocol_ != "http:" && inner.protocol_ != "https:")) return {};
            return inner.origin_value();
        }
        if (protocol_.empty() || host_.empty()) return {};
        return {OriginKind::Tuple, protocol_ + "//" + host_};
    }
    std::string origin() const { return std::string(origin_value().text()); }

    bool is_valid() const noexcept { return !protocol_.empty() && !hostname_.empty(); }
    bool is_empty() const noexcept { return href_.empty(); }
    bool parsed() const noexcept { return !protocol_.empty(); }

    bool operator==(const UrlValue& other) const { return href_ == other.href_; }
    bool operator!=(const UrlValue& other) const { return href_ != other.href_; }

    /// Component setters used by the `URL` interface. An unusable value is
    /// ignored (the serializations stay self-consistent) instead of throwing.
    void set_protocol(const std::string& value);
    void set_username(const std::string& value);
    void set_password(const std::string& value);
    void set_host(const std::string& value);
    void set_hostname(const std::string& value);
    void set_port(const std::string& value);
    void set_pathname(const std::string& value);
    void set_search(const std::string& value);
    void set_hash(const std::string& value);

    /// Create a UrlValue from individual components (Location profile).
    static UrlValue from_components(const std::string& protocol,
                                    const std::string& hostname,
                                    const std::string& port,
                                    const std::string& pathname,
                                    const std::string& search,
                                    const std::string& hash) {
        UrlValue url;
        url.protocol_ = protocol;
        url.hostname_ = hostname;
        url.port_ = port;
        url.pathname_ = pathname.empty() ? "/" : pathname;
        url.search_ = search;
        url.hash_ = hash;
        url.has_fragment_ = !hash.empty();
        url.authority_ = true;
        url.host_ = hostname;
        if (!port.empty()) url.host_ += ":" + port;
        url.Serialize();
        return url;
    }

    /// Resolve `input` against `base`; an empty base means an absolute URL only.
    static bool Resolve(const std::string& input, const std::string& base,
                        UrlValue* result) {
        UrlValue parsed_base;
        const UrlValue* reference = nullptr;
        if (!base.empty()) {
            if (!Parse(base, nullptr, &parsed_base)) return false;
            reference = &parsed_base;
        }
        return Parse(input, reference, result);
    }

    /// Remove the "." and ".." segments of a path.
    static std::string NormalizePath(const std::string& path);

private:
    static bool Parse(const std::string& input, const UrlValue* base, UrlValue* out);

    static std::string Trim(const std::string& text);
    static std::string Lower(const std::string& text);
    static std::string EncodeUserinfo(const std::string& text);
    static size_t SchemeEnd(const std::string& text);
    static bool Special(const std::string& protocol);
    static std::string DefaultPort(const std::string& protocol);
    static bool NormalizeHost(const std::string& input, std::string* host);
    static bool NormalizePort(const std::string& input, std::string* port);
    static bool SplitAuthority(const std::string& authority, std::string* username,
                               std::string* password, std::string* host, std::string* port,
                               bool allow_empty_host = false);
    static void Split(const std::string& text, char separator,
                      std::vector<std::string>* out);
    void RefreshHost();
    void Serialize();

    std::string href_;
    std::string protocol_;
    std::string username_;
    std::string password_;
    std::string host_;
    std::string hostname_;
    std::string port_;
    std::string pathname_;
    std::string search_;
    std::string hash_;
    bool authority_ = false;
    bool opaque_ = false;
    bool has_fragment_ = false;
};

inline std::string UrlValue::Lower(const std::string& text) {
    std::string out = text;
    for (char& unit : out)
        if (unit >= 'A' && unit <= 'Z') unit = char(unit - 'A' + 'a');
    return out;
}

inline std::string UrlValue::Trim(const std::string& text) {
    size_t begin = 0, end = text.size();
    auto space = [](unsigned char unit) { return unit <= 0x20; };
    while (begin < end && space(static_cast<unsigned char>(text[begin]))) ++begin;
    while (end > begin && space(static_cast<unsigned char>(text[end - 1]))) --end;
    return text.substr(begin, end - begin);
}

/// Index of the scheme separator, or npos when the text has no scheme.
inline size_t UrlValue::SchemeEnd(const std::string& text) {
    if (text.empty()) return std::string::npos;
    char first = text[0];
    if (!((first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z')))
        return std::string::npos;
    for (size_t index = 1; index < text.size(); ++index) {
        char unit = text[index];
        bool allowed = (unit >= 'a' && unit <= 'z') || (unit >= 'A' && unit <= 'Z') ||
                       (unit >= '0' && unit <= '9') || unit == '+' || unit == '-' ||
                       unit == '.';
        if (unit == ':') return index;
        if (!allowed) return std::string::npos;
    }
    return std::string::npos;
}

inline bool UrlValue::Special(const std::string& protocol) {
    return protocol == "http:" || protocol == "https:" || protocol == "ws:" ||
           protocol == "wss:" || protocol == "ftp:" || protocol == "file:";
}

inline std::string UrlValue::DefaultPort(const std::string& protocol) {
    if (protocol == "http:" || protocol == "ws:") return "80";
    if (protocol == "https:" || protocol == "wss:") return "443";
    if (protocol == "ftp:") return "21";
    return "";
}

inline bool UrlValue::NormalizeHost(const std::string& input, std::string* host) {
    if (input.empty()) return false;
    if (input.front() == '[') {
        if (input.back() != ']') return false;
        *host = Lower(input);
        return true;
    }
    std::string ascii;
    if (!url_idna::ToAscii(input, &ascii) || ascii.empty()) return false;
    std::string lowered = Lower(ascii);
    if (lowered.front() == '.' || lowered.back() == '.') return false;
    for (unsigned char unit : lowered) {
        bool allowed = (unit >= 'a' && unit <= 'z') || (unit >= '0' && unit <= '9') ||
                       unit == '.' || unit == '-' || unit == '_' || unit == '%' ||
                       unit == '~' || unit == '!';
        if (!allowed) return false;
    }
    *host = lowered;
    return true;
}

inline bool UrlValue::NormalizePort(const std::string& input, std::string* port) {
    if (input.empty()) { port->clear(); return true; }
    size_t begin = input[0] == '0' && input.size() > 1 ? input.find_first_not_of('0') : 0;
    std::string digits = begin == std::string::npos ? "0" : input.substr(begin);
    if (digits.empty() || digits.size() > 5) return false;
    unsigned number = 0;
    for (char unit : digits) {
        if (unit < '0' || unit > '9') return false;
        number = number * 10 + unsigned(unit - '0');
    }
    if (number > 65535) return false;
    *port = digits;
    return true;
}

inline bool UrlValue::SplitAuthority(const std::string& authority, std::string* username,
                                     std::string* password, std::string* host,
                                     std::string* port, bool allow_empty_host) {
    std::string rest = authority;
    size_t at = rest.rfind('@');
    if (at != std::string::npos) {
        std::string info = rest.substr(0, at);
        rest = rest.substr(at + 1);
        size_t colon = info.find(':');
        *username = colon == std::string::npos ? info : info.substr(0, colon);
        *password = colon == std::string::npos ? "" : info.substr(colon + 1);
    }
    std::string hostname = rest, port_text;
    if (!rest.empty() && rest.front() == '[') {
        size_t close = rest.find(']');
        if (close == std::string::npos) return false;
        hostname = rest.substr(0, close + 1);
        if (close + 1 < rest.size()) {
            if (rest[close + 1] != ':') return false;
            port_text = rest.substr(close + 2);
        }
    } else {
        size_t colon = rest.rfind(':');
        if (colon != std::string::npos) {
            hostname = rest.substr(0, colon);
            port_text = rest.substr(colon + 1);
        }
    }
    // file:///... has an empty authority by definition. Other schemes still
    // require a host when the authority marker is present.
    if (hostname.empty() && allow_empty_host) {
        host->clear();
        return port_text.empty();
    }
    if (hostname.empty()) return false;
    return NormalizeHost(hostname, host) && NormalizePort(port_text, port);
}

inline void UrlValue::Split(const std::string& text, char separator,
                            std::vector<std::string>* out) {
    std::string current;
    for (char unit : text) {
        if (unit == separator) { out->push_back(current); current.clear(); }
        else current.push_back(unit);
    }
    out->push_back(current);
}

inline std::string UrlValue::NormalizePath(const std::string& path) {
    if (path.empty()) return path;
    const bool absolute = path[0] == '/';
    std::vector<std::string> parts;
    Split(path, '/', &parts);
    std::vector<std::string> out;
    for (size_t index = 0; index < parts.size(); ++index) {
        const std::string& part = parts[index];
        const bool last = index + 1 == parts.size();
        auto dot_kind = [](const std::string& value) {
            std::string lower = value;
            for (char& c : lower)
                if (c >= 'A' && c <= 'F') c = char(c - 'A' + 'a');
            if (lower == "." || lower == "%2e") return 1;
            if (lower == ".." || lower == ".%2e" || lower == "%2e." ||
                lower == "%2e%2e") return 2;
            return 0;
        };
        const int dot = dot_kind(part);
        if (dot == 1) {
            if (last) out.emplace_back();
            continue;
        }
        if (dot == 2) {
            if (absolute) {
                // Remove exactly one preceding segment, including an empty
                // segment. This is what makes /a//../b become /a/b.
                if (out.size() > 1 || (!out.empty() && out.front() != ""))
                    out.pop_back();
            } else if (!out.empty() && out.back() != "..") out.pop_back();
            else if (!absolute) out.emplace_back("..");
            if (last) out.emplace_back();
            continue;
        }
        out.push_back(part);
    }
    std::string joined;
    for (size_t index = 0; index < out.size(); ++index) {
        if (index) joined.push_back('/');
        joined += out[index];
    }
    return joined;
}

inline bool UrlValue::Parse(const std::string& raw, const UrlValue* base, UrlValue* out) {
    std::string input = Trim(raw);
    bool has_fragment = false, has_query = false;
    std::string fragment, query;
    size_t hash = input.find('#');
    if (hash != std::string::npos) {
        fragment = input.substr(hash + 1);
        input.erase(hash);
        has_fragment = true;
    }
    size_t question = input.find('?');
    if (question != std::string::npos) {
        query = input.substr(question + 1);
        input.erase(question);
        has_query = true;
    }

    UrlValue url;
    size_t colon = SchemeEnd(input);
    if (colon != std::string::npos) {
        url.protocol_ = Lower(input.substr(0, colon)) + ":";
        std::string rest = input.substr(colon + 1);
        if (rest.rfind("//", 0) == 0) {
            url.authority_ = true;
            std::string after = rest.substr(2);
            size_t stop = after.find('/');
            if (Special(url.protocol_)) {
                size_t slash = after.find('\\');
                if (slash != std::string::npos && (stop == std::string::npos || slash < stop))
                    stop = slash;
            }
            std::string authority = stop == std::string::npos ? after : after.substr(0, stop);
            std::string path = stop == std::string::npos ? "" : after.substr(stop);
            if (!SplitAuthority(authority, &url.username_, &url.password_, &url.hostname_,
                                &url.port_, url.protocol_ == "file:")) return false;
            if (Special(url.protocol_))
                for (char& c : path) if (c == '\\') c = '/';
            url.pathname_ = NormalizePath(path);
            if (url.pathname_.empty()) url.pathname_ = "/";
        } else {
            if (Special(url.protocol_)) return false;
            url.opaque_ = !rest.empty() && rest[0] != '/';
            url.pathname_ = url.opaque_ ? rest : NormalizePath(rest);
        }
    } else {
        if (!base || base->protocol_.empty()) return false;
        url.protocol_ = base->protocol_;
        url.username_ = base->username_;
        url.password_ = base->password_;
        url.hostname_ = base->hostname_;
        url.port_ = base->port_;
        url.authority_ = base->authority_;
        url.opaque_ = base->opaque_;
        if (input.rfind("//", 0) == 0) {
            url.authority_ = true;
            std::string after = input.substr(2);
            size_t stop = after.find('/');
            if (Special(url.protocol_)) {
                size_t slash = after.find('\\');
                if (slash != std::string::npos && (stop == std::string::npos || slash < stop))
                    stop = slash;
            }
            std::string authority = stop == std::string::npos ? after : after.substr(0, stop);
            std::string path = stop == std::string::npos ? "" : after.substr(stop);
            url.username_.clear();
            url.password_.clear();
            if (!SplitAuthority(authority, &url.username_, &url.password_, &url.hostname_,
                                &url.port_, url.protocol_ == "file:")) return false;
            if (Special(url.protocol_))
                for (char& c : path) if (c == '\\') c = '/';
            url.pathname_ = NormalizePath(path);
            if (url.pathname_.empty()) url.pathname_ = "/";
        } else if (!input.empty() && input[0] == '/') {
            if (Special(url.protocol_))
                for (char& c : input) if (c == '\\') c = '/';
            url.pathname_ = NormalizePath(input);
            if (url.pathname_.empty()) url.pathname_ = "/";
        } else if (input.empty()) {
            url.pathname_ = base->pathname_;
            // An empty or fragment-only reference keeps the base's query.
            if (!has_query && !base->search_.empty()) {
                has_query = true;
                query = base->search_.substr(1);
            }
        } else {
            std::string directory = base->pathname_;
            size_t slash = directory.rfind('/');
            std::string prefix = slash == std::string::npos ? "/" : directory.substr(0, slash + 1);
            if (Special(url.protocol_))
                for (char& c : input) if (c == '\\') c = '/';
            url.pathname_ = NormalizePath(prefix + input);
            if (url.pathname_.empty()) url.pathname_ = "/";
        }
    }

    if (!url.port_.empty() && url.port_ == DefaultPort(url.protocol_)) url.port_.clear();
    // Firefox's standard-URL parser terminates a parsed query at NUL; the
    // component setter encodes NUL normally (both verified in content Firefox).
    const auto nul = query.find('\0');
    if (Special(url.protocol_) && nul != std::string::npos) {
        query.erase(nul);
        has_fragment = false;
    }
    url.search_ = has_query ? "?" + query : "";
    url.has_fragment_ = has_fragment;
    url.hash_ = has_fragment && !fragment.empty() ? "#" + fragment : "";
    if (url.pathname_.empty() && url.authority_) url.pathname_ = "/";
    url.RefreshHost();
    url.Serialize();
    if (url.href_.empty()) return false;
    *out = url;
    return true;
}

inline void UrlValue::RefreshHost() {
    host_ = hostname_;
    if (!port_.empty()) host_ += ":" + port_;
}

inline void UrlValue::Serialize() {
    search_ = EncodeUrlQuery(search_, Special(protocol_));
    pathname_ = EncodeUrlComponent(pathname_, false, opaque_, Special(protocol_));
    std::string fragment = EncodeUrlComponent(hash_.empty() ? "" : hash_.substr(1), true);
    hash_ = has_fragment_ && !fragment.empty() ? "#" + fragment : "";
    href_ = protocol_;
    if (authority_) {
        href_ += "//";
        if (!username_.empty() || !password_.empty()) {
            href_ += username_;
            if (!password_.empty()) href_ += ":" + password_;
            href_ += "@";
        }
        href_ += host_;
    }
    href_ += pathname_;
    href_ += search_;
    if (has_fragment_) href_ += "#" + fragment;
}

inline void UrlValue::set_protocol(const std::string& value) {
    std::string text = value;
    if (!text.empty() && text.back() == ':') text.pop_back();
    if (text.empty()) return;
    if (SchemeEnd(text + ":") != text.size()) return;
    protocol_ = Lower(text) + ":";
    if (!port_.empty() && port_ == DefaultPort(protocol_)) port_.clear();
    RefreshHost();
    Serialize();
}

// Firefox nsEscape userinfo encode set; '%' remains already-escaped input.
inline std::string UrlValue::EncodeUserinfo(const std::string& text) {
    std::string out;
    static constexpr char hex[] = "0123456789ABCDEF";
    for (unsigned char c : text) {
        const bool keep = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || std::string("!$&()*+,-_~'.%").find(char(c)) != std::string::npos;
        if (keep) out += char(c);
        else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
    }
    return out;
}
inline void UrlValue::set_username(const std::string& value) {
    if (!authority_ || hostname_.empty() || protocol_ == "file:") return;
    username_ = EncodeUserinfo(value);
    Serialize();
}
inline void UrlValue::set_password(const std::string& value) {
    if (!authority_ || hostname_.empty() || protocol_ == "file:") return;
    password_ = EncodeUserinfo(value);
    Serialize();
}

inline void UrlValue::set_host(const std::string& value) {
    std::string username, password, host, port;
    if (!SplitAuthority(value, &username, &password, &host, &port,
                        protocol_ == "file:")) return;
    if (!username.empty() || !password.empty()) return;
    if (host.empty() && !Special(protocol_)) return;
    hostname_ = host;
    port_ = port;
    if (!port_.empty() && port_ == DefaultPort(protocol_)) port_.clear();
    RefreshHost();
    Serialize();
}

inline void UrlValue::set_hostname(const std::string& value) {
    std::string host;
    if (!NormalizeHost(value, &host)) return;
    hostname_ = host;
    RefreshHost();
    Serialize();
}

inline void UrlValue::set_port(const std::string& value) {
    std::string port;
    if (!NormalizePort(value, &port)) return;
    if (!port.empty() && port == DefaultPort(protocol_)) port.clear();
    port_ = port;
    RefreshHost();
    Serialize();
}

inline void UrlValue::set_pathname(const std::string& value) {
    if (opaque_) return;
    std::string cleaned = value;
    if (Special(protocol_))
        for (char& c : cleaned) if (c == '\\') c = '/';
    if (!cleaned.empty() && cleaned[0] != '/' && !protocol_.empty()) cleaned = "/" + cleaned;
    std::string path = NormalizePath(cleaned);
    if (path.empty() && (authority_ || !protocol_.empty())) path = "/";
    pathname_ = path;
    Serialize();
}

inline void UrlValue::set_search(const std::string& value) {
    if (value.empty() || value == "?") search_.clear();
    else search_ = value[0] == '?' ? value : "?" + value;
    Serialize();
}

inline void UrlValue::set_hash(const std::string& value) {
    if (value.empty()) { hash_.clear(); has_fragment_ = false; }
    else { hash_ = value[0] == '#' ? value : "#" + value; has_fragment_ = true; }
    Serialize();
}

}  // namespace ruyijs::domain::browser_environment
