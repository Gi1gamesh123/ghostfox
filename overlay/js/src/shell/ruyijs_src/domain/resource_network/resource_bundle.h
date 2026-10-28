#pragma once

#include "domain/shared/entity.h"
#include "domain/shared/aggregate_root.h"
#include "domain/shared/domain_event.h"
#include "domain/resource_network/http_types.h"
#include "domain/resource_network/net_log.h"
#include "domain/resource_network/live_request.h"
#include "domain/resource_network/resource.h"
#include "domain/resource_network/resource_services.h"
#include "domain/resource_network/cache_storage.h"
#include <stdexcept>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace ruyijs::domain::resource_network {

/// Aggregate Root: ResourceBundle
/// Manages the offline resource bundle and network log.
/// All XHR/fetch/external resource requests are matched against this bundle.
class ResourceBundleAggregate : public shared::AggregateRoot<uint64_t> {
public:
    explicit ResourceBundleAggregate(uint64_t id = 1)
        : AggregateRoot<uint64_t>(id) {}

    void add_resource(const std::string& url, const std::string& body,
                      HttpStatus status = HttpStatus(200), HttpHeaders headers = {},
                      ResourceBodyKind kind = ResourceBodyKind::Text) {
        for (auto& [rid, res] : resources_) {
            if (res->url() != url) continue;
            auto next = std::make_unique<Resource>(rid, url, body, status, std::move(headers), kind);
            const size_t retained = resource_bytes_ - res->bytes() + next->bytes();
            res.swap(next);
            resource_bytes_ = retained;
            return;
        }
        if (next_resource_id_ == UINT64_MAX) throw std::length_error("Resource id exhausted");
        const auto id = next_resource_id_ + 1;
        auto next = std::make_unique<Resource>(id, url, body, status, std::move(headers), kind);
        const size_t retained = next->bytes();
        resources_.emplace(id, std::move(next));
        next_resource_id_ = id;
        resource_bytes_ += retained;
    }
    size_t resource_bytes() const noexcept { return resource_bytes_; }
    void clear_resources() noexcept { resources_.clear(); resource_bytes_ = 0; }

    const Resource* find_by_url(const std::string& url) const {
        for (const auto& [id, res] : resources_) {
            if (res->url() == url) return res.get();
        }
        return nullptr;
    }

    const Resource* find_by_path(const std::string& url) const {
        auto* exact = find_by_url(url);
        if (exact) return exact;
        auto query_pos = url.find('?');
        if (query_pos != std::string::npos) return find_by_url(url.substr(0, query_pos));
        return nullptr;
    }

    bool has_resource(const std::string& url) const {
        return find_by_url(url) != nullptr;
    }

    size_t resource_count() const noexcept { return resources_.size(); }

    CacheStorageState& cache_storage() noexcept { return cache_storage_; }
    const CacheStorageState& cache_storage() const noexcept { return cache_storage_; }

    NetLog& net_log() noexcept { return net_log_; }
    const NetLog& net_log() const noexcept { return net_log_; }

    const Resource* record_and_match(HttpMethod method, const std::string& url,
                                     const std::string& body = "", uint64_t* recorded_id = nullptr) {
        const auto log_id = net_log_.record(method, url, body);
        if (recorded_id) *recorded_id = log_id;
        auto* resource = find_by_path(url);
        if (resource && log_id) net_log_.mark_matched(log_id, resource->status().code());
        return resource;
    }

    uint64_t open_request(HttpMethod method, const std::string& url, const std::string& body = "") {
        if (!live_.can_open()) return 0;
        auto entry = net_log_.prepare(method, url, body);
        if (!entry) return 0;
        const auto id = live_.open(method, url, entry->id());
        if (!id) return 0;
        net_log_.commit(std::move(*entry));
        net_log_.pin();
        return id;
    }

    const LiveRequest* find_request(uint64_t id) const { return live_.find(id); }

    bool send_request(uint64_t id, uint32_t generation, uint32_t hop) {
        return live_.send(id, generation, hop);
    }

    bool redirect_request(uint64_t id, uint32_t generation, uint32_t hop,
                          int status, std::string next_url) {
        return live_.redirect(id, generation, hop, status, std::move(next_url));
    }

    bool complete_request(uint64_t id, uint32_t generation, uint32_t hop, int status) {
        const auto* current = live_.find(id);
        if (!current || !net_log_.find(current->log_id())) return false;
        const auto log_id = current->log_id();
        if (!live_.complete(id, generation, hop, status)) return false;
        net_log_.mark_matched(log_id, status);
        net_log_.unpin();
        return true;
    }

    bool fail_request(uint64_t id, uint32_t generation, uint32_t hop) {
        if (!live_.fail(id, generation, hop)) return false;
        net_log_.unpin();
        return true;
    }

    bool cancel_request(uint64_t id, uint32_t generation) {
        if (!live_.cancel(id, generation)) return false;
        net_log_.unpin();
        return true;
    }

    bool forget_request(uint64_t id) noexcept { return live_.forget(id); }

private:
    size_t resource_bytes_ = 0;
    uint64_t next_resource_id_ = 0;
    std::unordered_map<ResourceId, std::unique_ptr<Resource>> resources_;
    CacheStorageState cache_storage_;
    NetLog net_log_;
    LiveRequestTable live_;
};

}
