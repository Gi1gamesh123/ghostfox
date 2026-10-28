#pragma once

#include <algorithm>
#include <cstdint>
#include "domain/resource_network/http_headers.h"
#include <string>
#include <vector>

namespace ruyijs::domain::resource_network {

/// Value Object: HTTP method.
enum class HttpMethod {
    GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS,
};

inline const char* to_string(HttpMethod m) {
    switch (m) {
        case HttpMethod::GET: return "GET";
        case HttpMethod::POST: return "POST";
        case HttpMethod::PUT: return "PUT";
        case HttpMethod::DELETE: return "DELETE";
        case HttpMethod::PATCH: return "PATCH";
        case HttpMethod::HEAD: return "HEAD";
        case HttpMethod::OPTIONS: return "OPTIONS";
    }
    return "GET";
}

inline HttpMethod method_from_string(const std::string& s) {
    if (s == "POST") return HttpMethod::POST;
    if (s == "PUT") return HttpMethod::PUT;
    if (s == "DELETE") return HttpMethod::DELETE;
    if (s == "PATCH") return HttpMethod::PATCH;
    if (s == "HEAD") return HttpMethod::HEAD;
    if (s == "OPTIONS") return HttpMethod::OPTIONS;
    return HttpMethod::GET;
}

/// Value Object: HTTP status code.
class HttpStatus {
public:
    HttpStatus() = default;
    explicit HttpStatus(int code) : code_(code) {}

    int code() const noexcept { return code_; }
    bool is_ok() const noexcept { return code_ >= 200 && code_ < 300; }
    bool is_redirect() const noexcept { return code_ >= 300 && code_ < 400; }
    bool is_client_error() const noexcept { return code_ >= 400 && code_ < 500; }
    bool is_server_error() const noexcept { return code_ >= 500; }

    std::string text() const {
        switch (code_) {
            case 200: return "OK";
            case 201: return "Created";
            case 204: return "No Content";
            case 301: return "Moved Permanently";
            case 302: return "Found";
            case 304: return "Not Modified";
            case 400: return "Bad Request";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 500: return "Internal Server Error";
            default: return "Unknown";
        }
    }

    bool operator==(const HttpStatus& o) const { return code_ == o.code_; }

private:
    int code_ = 200;
};

} // namespace ruyijs::domain::resource_network
