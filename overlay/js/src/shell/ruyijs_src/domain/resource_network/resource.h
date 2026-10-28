#pragma once
#include "domain/shared/entity.h"
#include "domain/resource_network/http_types.h"

namespace ruyijs::domain::resource_network {
using ResourceId = uint64_t;
enum class ResourceBodyKind { Text, Bytes };

/// Entity: An offline resource (injected HTTP response).
/// Represents a URL → response mapping in the offline bundle.
class Resource : public shared::Entity<ResourceId> {
public:
    Resource(ResourceId id, std::string url, std::string body,
             HttpStatus status = HttpStatus(200), HttpHeaders headers = {},
             ResourceBodyKind kind = ResourceBodyKind::Text)
        : Entity<ResourceId>(id),
          url_(std::move(url)),
          body_(std::move(body)),
          status_(status),
          headers_(std::move(headers)), kind_(kind) {}

    const std::string& url() const noexcept { return url_; }
    const std::string& body() const noexcept { return body_; }
    HttpStatus status() const noexcept { return status_; }
    const HttpHeaders& headers() const noexcept { return headers_; }

    ResourceBodyKind kind() const noexcept { return kind_; }
    size_t bytes() const noexcept { return url_.size() + body_.size() + headers_.bytes(); }

    void set_body(std::string body) { body_ = std::move(body); }
    void set_status(HttpStatus status) { status_ = status; }
    void set_headers(HttpHeaders headers) { headers_ = std::move(headers); }

private:
    std::string url_;
    std::string body_;
    HttpStatus status_;
    HttpHeaders headers_;
    ResourceBodyKind kind_;
};

}
