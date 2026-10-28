#pragma once

#include "domain/resource_network/http_types.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ruyijs::domain::resource_network {

enum class RequestPhase { Open, Sent, Completed, Failed, Cancelled };

struct HopRecord {
    uint32_t index = 0;
    std::string url;
    int status = 0;
};

class LiveRequest {
public:
    uint64_t id() const noexcept { return id_; }
    uint32_t generation() const noexcept { return generation_; }
    uint32_t hop() const noexcept { return hops_.back().index; }
    RequestPhase phase() const noexcept { return phase_; }
    HttpMethod method() const noexcept { return method_; }
    const std::string& url() const noexcept { return hops_.back().url; }
    uint64_t log_id() const noexcept { return log_id_; }
    const std::vector<HopRecord>& hops() const noexcept { return hops_; }
    bool active() const noexcept {
        return phase_ == RequestPhase::Open || phase_ == RequestPhase::Sent;
    }

private:
    friend class LiveRequestTable;
    uint64_t id_ = 0;
    uint32_t generation_ = 1;
    HttpMethod method_ = HttpMethod::GET;
    uint64_t log_id_ = 0;
    RequestPhase phase_ = RequestPhase::Open;
    std::vector<HopRecord> hops_;
};

class LiveRequestTable {
public:
    static constexpr size_t kActiveLimit = 64;
    static constexpr size_t kRetainedLimit = 1024, kHopLimit = 20;
    static constexpr size_t kUrlLimit = 16384, kByteLimit = 16 * 1024 * 1024;

    bool can_open() const noexcept {
        return active_ < kActiveLimit && items_.size() < kRetainedLimit && next_id_ != UINT64_MAX;
    }
    size_t active() const noexcept { return active_; }
    size_t retained() const noexcept { return items_.size(); }
    size_t bytes() const noexcept { return bytes_; }

    uint64_t open(HttpMethod method, std::string url, uint64_t log_id) {
        if (!can_open() || log_id == 0 || url.empty() || url.size() > kUrlLimit ||
            url.size() > kByteLimit - bytes_) return 0;
        LiveRequest request;
        request.id_ = next_id_ + 1;
        request.method_ = method;
        request.log_id_ = log_id;
        const size_t retained_bytes = url.size();
        request.hops_.push_back(HopRecord{0, std::move(url), 0});
        const auto id = request.id_;
        items_.emplace(id, std::move(request));
        next_id_ = id;
        bytes_ += retained_bytes;
        ++active_;
        return id;
    }

    const LiveRequest* find(uint64_t id) const {
        auto it = items_.find(id);
        return it == items_.end() ? nullptr : &it->second;
    }

    bool send(uint64_t id, uint32_t generation, uint32_t hop) {
        auto* request = match(id, generation, hop);
        if (!request || request->phase_ != RequestPhase::Open) return false;
        request->phase_ = RequestPhase::Sent;
        return true;
    }

    bool redirect(uint64_t id, uint32_t generation, uint32_t hop, int status, std::string next_url) {
        auto* request = match(id, generation, hop);
        if (!request || request->generation_ == UINT32_MAX || hop == UINT32_MAX ||
            request->hops_.size() >= kHopLimit || next_url.empty() ||
            next_url.size() > kUrlLimit || next_url.size() > kByteLimit - bytes_) return false;
        const size_t retained_bytes = next_url.size();
        request->hops_.push_back(HopRecord{hop + 1, std::move(next_url), 0});
        request->hops_[request->hops_.size() - 2].status = status;
        bytes_ += retained_bytes;
        ++request->generation_;
        request->phase_ = RequestPhase::Sent;
        return true;
    }

    bool complete(uint64_t id, uint32_t generation, uint32_t hop, int status) {
        return finish(id, generation, hop, status, RequestPhase::Completed);
    }

    bool fail(uint64_t id, uint32_t generation, uint32_t hop) {
        return finish(id, generation, hop, 0, RequestPhase::Failed);
    }

    bool cancel(uint64_t id, uint32_t generation) {
        auto it = items_.find(id);
        if (it == items_.end() || !it->second.active() || it->second.generation_ != generation) return false;
        it->second.phase_ = RequestPhase::Cancelled;
        --active_;
        return true;
    }
    bool forget(uint64_t id) noexcept {
        auto it = items_.find(id);
        if (it == items_.end() || it->second.active()) return false;
        for (const auto& hop : it->second.hops_) bytes_ -= hop.url.size();
        items_.erase(it);
        return true;
    }

private:
    LiveRequest* match(uint64_t id, uint32_t generation, uint32_t hop) {
        auto it = items_.find(id);
        if (it == items_.end() || !it->second.active()) return nullptr;
        if (it->second.generation_ != generation || it->second.hop() != hop) return nullptr;
        return &it->second;
    }

    bool finish(uint64_t id, uint32_t generation, uint32_t hop, int status, RequestPhase phase) {
        auto* request = match(id, generation, hop);
        if (!request) return false;
        request->hops_.back().status = status;
        request->phase_ = phase;
        --active_;
        return true;
    }

    uint64_t next_id_ = 0;
    size_t active_ = 0;
    size_t bytes_ = 0;
    std::unordered_map<uint64_t, LiveRequest> items_;
};

}
