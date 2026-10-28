#pragma once

#include "domain/resource_network/http_types.h"

#include <cstdint>
#include <numeric>
#include <string>
#include <optional>
#include <vector>

namespace ruyijs::domain::resource_network {

class NetLogEntry {
public:
    NetLogEntry(uint64_t id, HttpMethod method, std::string url, std::string request_body = "")
        : id_(id), method_(method), url_(std::move(url)), request_body_(std::move(request_body)) {}

    uint64_t id() const noexcept { return id_; }
    HttpMethod method() const noexcept { return method_; }
    const std::string& url() const noexcept { return url_; }
    const std::string& request_body() const noexcept { return request_body_; }

    int response_status() const noexcept { return response_status_; }
    void set_response_status(int status) noexcept { response_status_ = status; }

    bool matched() const noexcept { return matched_; }
    const std::vector<std::string>& cookie_names() const noexcept {return cookie_names_;}
    void set_cookie_names(std::vector<std::string> names) {cookie_names_=std::move(names);}
    void set_matched(bool matched) noexcept { matched_ = matched; }
    const std::string& error() const noexcept { return error_; }
    void set_error(std::string error) { error_ = std::move(error); }
    size_t bytes() const noexcept {
        return url_.size()+request_body_.size()+error_.size()+
            std::accumulate(cookie_names_.begin(),cookie_names_.end(),size_t(0),
                [](size_t total,const auto& name){return total+name.size();});
    }

private:
    uint64_t id_;
    HttpMethod method_;
    std::string url_;
    std::string request_body_;
    int response_status_ = 0;
    bool matched_ = false;
    std::vector<std::string> cookie_names_;
    std::string error_;
};

class NetLog {
public:
    NetLog() = default;
    static constexpr size_t kEntryLimit = 4096, kByteLimit = 16 * 1024 * 1024;

private:
    friend class ResourceBundleAggregate;
    std::optional<NetLogEntry> prepare(HttpMethod method, const std::string& url,
                                     const std::string& body = "") {
        if (next_id_ == UINT64_MAX || entries_.size() >= kEntryLimit ||
            url.size() > kByteLimit || body.size() > kByteLimit - url.size() ||
            bytes_ > kByteLimit - url.size() - body.size()) return std::nullopt;
        NetLogEntry entry(next_id_ + 1, method, url, body);
        entries_.reserve(entries_.size() + 1);
        return entry;
    }
    uint64_t commit(NetLogEntry entry) noexcept {
        next_id_ = entry.id();
        bytes_ += entry.bytes();
        entries_.push_back(std::move(entry));
        return next_id_;
    }
    void pin() noexcept { ++pins_; }
    void unpin() noexcept { if (pins_) --pins_; }

public:
    uint64_t record(HttpMethod method, const std::string& url, const std::string& body = "") {
        auto entry = prepare(method, url, body);
        return entry ? commit(std::move(*entry)) : 0;
    }

    const std::vector<NetLogEntry>& entries() const noexcept { return entries_; }
    size_t size() const noexcept { return entries_.size(); }
    bool empty() const noexcept { return entries_.empty(); }

    bool clear() noexcept {
        if (pins_) return false;
        entries_.clear(); bytes_ = 0; return true;
    }
    size_t bytes() const noexcept { return bytes_; }
    const NetLogEntry* find(uint64_t id) const noexcept {
        for (const auto& entry : entries_) if (entry.id() == id) return &entry;
        return nullptr;
    }

    bool mark_matched(uint64_t id, int status) {
        for (auto& entry : entries_) {
            if (entry.id() != id) continue;
            entry.set_matched(true);
            entry.set_response_status(status);
            return true;
        }
        return false;
    }

    void mark_last_matched(int status) {
        if (!entries_.empty()) {
            entries_.back().set_matched(true);
            entries_.back().set_response_status(status);
        }
    }

    bool mark_response(uint64_t id, int status) noexcept {
        for (auto& entry : entries_) {
            if (entry.id() != id) continue;
            entry.set_response_status(status);
            return true;
        }
        return false;
    }
    void mark_error(uint64_t id, std::string error) {
        if (error.size() > kByteLimit) return;
        for (auto& entry : entries_) {
            if (entry.id() != id) continue;
            if (bytes_ > kByteLimit - error.size() + entry.error().size()) return;
            bytes_ += error.size() - entry.error().size();
            entry.set_error(std::move(error));
            return;
        }
    }
    void mark_cookies(uint64_t id,std::vector<std::string> names) {
        size_t size=0;for(const auto& name:names)size+=name.size();
        if(size>kByteLimit||bytes_>kByteLimit-size||names.size()>256)return;
        for(auto& entry:entries_)if(entry.id()==id) {
            bytes_-=entry.bytes();entry.set_cookie_names(std::move(names));bytes_+=entry.bytes();return;
        }
    }

private:
    uint64_t next_id_ = 0;
    size_t bytes_ = 0, pins_ = 0;
    std::vector<NetLogEntry> entries_;
};

}
