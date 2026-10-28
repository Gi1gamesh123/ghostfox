#pragma once
#include "domain/resource_network/resource.h"
#include "domain/shared/domain_event.h"
#include <memory>

namespace ruyijs::domain::resource_network {
/// Domain Service: Resource URL matching.
class ResourceMatcherService {
public:
    static std::string resolve_url(const std::string& base, const std::string& relative) {
        if (relative.empty()) return base;
        if (relative.find("://") != std::string::npos) return relative;
        if (relative.rfind("//", 0) == 0) {
            auto proto_end = base.find("://");
            if (proto_end != std::string::npos) return base.substr(0, proto_end + 1) + relative;
            return "https:" + relative;
        }
        auto proto_end = base.find("://");
        if (proto_end == std::string::npos) return relative;
        auto path_start = base.find('/', proto_end + 3);
        std::string origin = (path_start != std::string::npos) ? base.substr(0, path_start) : base;
        if (relative[0] == '/') return origin + relative;
        std::string base_dir = base;
        auto last_slash = base_dir.rfind('/');
        if (last_slash != std::string::npos && last_slash > proto_end + 2) {
            base_dir = base_dir.substr(0, last_slash + 1);
        } else {
            base_dir = origin + "/";
        }
        return base_dir + relative;
    }
};

class ResourceRepository {
public:
    virtual ~ResourceRepository() = default;
    virtual void save(ResourceId id, std::unique_ptr<Resource> resource) = 0;
    virtual const Resource* find_by_url(const std::string& url) const = 0;
    virtual size_t count() const = 0;
    virtual void clear() = 0;
};

class ResourceInjectedEvent : public shared::DomainEvent {
public:
    explicit ResourceInjectedEvent(std::string url)
        : DomainEvent("ResourceInjected"), url_(std::move(url)) {}
    const std::string& url() const noexcept { return url_; }
private:
    std::string url_;
};

class RequestInterceptedEvent : public shared::DomainEvent {
public:
    RequestInterceptedEvent(std::string method, std::string url, bool matched)
        : DomainEvent("RequestIntercepted"),
          method_(std::move(method)), url_(std::move(url)), matched_(matched) {}
    const std::string& method() const noexcept { return method_; }
    const std::string& url() const noexcept { return url_; }
    bool matched() const noexcept { return matched_; }
private:
    std::string method_;
    std::string url_;
    bool matched_;
};
}
