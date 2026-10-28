#pragma once
#include <string>

namespace ruyijs::domain::browser_environment {

// Encode a URL path or fragment over the UTF-8 bytes already produced by the
// WebIDL string conversion. Percent escapes are deliberately left intact: a
// setter may be given an already escaped component and Firefox does not
// double-escape it. CR/LF/TAB are removed by the URL parser and setters.
inline std::string EncodeUrlComponent(const std::string& text, bool fragment,
                                      bool opaque_path = false,
                                      bool special_path = true) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(text.size());
    for (unsigned char unit : text) {
        if (unit == '\t' || unit == '\n' || unit == '\r') continue;
        bool keep = false;
        if (opaque_path && unit >= 0x20 && unit <= 0x7e && unit != '%') {
            // Non-hierarchical URL paths keep printable ASCII, including
            // spaces and quotes. '%' remains the already-escaped marker.
            keep = true;
        } else if (unit >= 0x21 && unit <= 0x7e && unit != '"' && unit != '<' &&
                   unit != '>' && unit != '`' && unit != '%') {
            if (fragment) {
                // Fragment percent-encode set leaves delimiters such as '#',
                // '?', '\\', '^' and braces visible.
                keep = true;
            } else {
                // Hierarchical path uses the URL pchar set. '?' and '#'
                // become data when assigned through pathname.
                keep = (unit >= 'a' && unit <= 'z') ||
                        (unit >= 'A' && unit <= 'Z') ||
                        (unit >= '0' && unit <= '9') ||
                        (std::string("!$&'()*+,-./:;=@[]|_~").find(char(unit)) !=
                         std::string::npos || (!special_path && unit == '\\'));
            }
        }
        if (unit == '%') keep = true;
        if (keep) out += char(unit);
        else {
            out += '%';
            out += hex[unit >> 4];
            out += hex[unit & 15];
        }
    }
    return out;
}

// URL query percent-encode set over UTF-8 bytes. Existing '%' sequences stay
// untouched; form-urlencoded '+' handling belongs to URLSearchParams instead.
inline std::string EncodeUrlQuery(const std::string& text, bool special) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(text.size());
    for (unsigned char unit : text) {
        if (unit == '\t' || unit == '\n' || unit == '\r') continue;
        if (unit <= 0x20 || unit > 0x7e || unit == '"' || unit == '#' ||
            unit == '<' || unit == '>' || (special && unit == '\'')) {
            out += '%';
            out += hex[unit >> 4];
            out += hex[unit & 15];
        } else out += char(unit);
    }
    return out;
}

}  // namespace ruyijs::domain::browser_environment
