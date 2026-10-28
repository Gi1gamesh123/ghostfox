#pragma once

#include "domain/browser_environment/url_value.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace ruyijs::domain::browser_environment {

/// Entity: Location information (current page URL).
/// Wraps UrlValue and adds navigation methods semantics.
class LocationInfo {
public:
    LocationInfo() = default;

    explicit LocationInfo(const std::string& href) : url_(href) {}

    // URL component accessors (delegate to UrlValue)
    const std::string& href() const noexcept { return url_.href(); }
    const std::string& protocol() const noexcept { return url_.protocol(); }
    const std::string& host() const noexcept { return url_.host(); }
    const std::string& hostname() const noexcept { return url_.hostname(); }
    const std::string& port() const noexcept { return url_.port(); }
    const std::string& pathname() const noexcept { return url_.pathname(); }
    const std::string& search() const noexcept { return url_.search(); }
    const std::string& hash() const noexcept { return url_.hash(); }
    std::string origin() const { return url_.origin(); }

    /// Navigate to a new URL (replaces current location)
    void navigate(const std::string& new_href) {
        url_ = UrlValue(new_href);
    }

    /// Update from individual components
    void set_components(const std::string& protocol,
                        const std::string& hostname,
                        const std::string& port,
                        const std::string& pathname,
                        const std::string& search,
                        const std::string& hash) {
        url_ = UrlValue::from_components(protocol, hostname, port, pathname, search, hash);
    }

    const UrlValue& url() const noexcept { return url_; }

    /// Apply default (about:blank)
    void apply_defaults() {
        url_ = UrlValue("https://example.com/");
    }

private:
    UrlValue url_;
};

/// Value Object: one session-history entry.
///
/// The structured-clone state is kept as opaque bytes: the domain owns the
/// entry identity, URL, same-document flag and serialized state, but has no
/// JavaScript dependency. The engine adapter encodes the value before the
/// commit and decodes it when the state is read back. An empty blob marks an
/// entry whose state has never been set, which reads back as `null`.
struct HistoryEntry {
    uint64_t id = 0;        // stable across replace: the Navigation API key
    uint64_t revision = 0;  // bumped by replace: the Navigation API id changes
    std::string url;
    bool same_document = false;
    std::vector<uint8_t> state;             // history.state
    std::vector<uint8_t> navigation_state;  // NavigationHistoryEntry.getState(), separate
};

/// The session history's scrollRestoration enum (Firefox default "auto").
enum class ScrollRestoration { Auto, Manual };

/// Entity: History information.
///
/// The one session-history owner: a bounded entry list plus the current index.
/// push drops the forward entries and the oldest entry once the cap is
/// exceeded, so the length never grows without bound.
class HistoryInfo {
public:
    static constexpr std::size_t kMaxEntries = 50;

    HistoryInfo() = default;

    int length() const noexcept { return static_cast<int>(entries_.size()); }
    std::size_t index() const noexcept { return index_; }
    const HistoryEntry& current() const noexcept { return entries_[index_]; }

    /// pushState: append after the current entry (dropping forward entries),
    /// then drop the oldest entry once the bounded capacity is exceeded.
    void push_state(HistoryEntry entry) {
        if (entry.id == 0) entry.id = next_id_++;
        entries_.resize(index_ + 1);
        entries_.push_back(std::move(entry));
        if (entries_.size() > kMaxEntries) entries_.erase(entries_.begin());
        index_ = entries_.size() - 1;
    }

    /// replaceState: replace the current entry without changing the length,
    /// keeping the replaced entry's stable identity.
    void replace_state(HistoryEntry entry) {
        entry.id = entries_[index_].id;
        entry.revision = entries_[index_].revision + 1;
        entries_[index_] = std::move(entry);
    }

    const std::vector<HistoryEntry>& entries() const noexcept { return entries_; }
    /// navigation.updateCurrentEntry({state}): the entry's identity is unchanged.
    void set_navigation_state(std::vector<uint8_t> state) { entries_[index_].navigation_state = std::move(state); }

    /// Traverse `delta` entries; an out-of-range or zero delta is a no-op.
    /// Returns whether the current entry changed.
    bool go(int delta) {
        if (delta == 0) return false;
        long long target = static_cast<long long>(index_) + delta;
        if (target < 0 || target >= static_cast<long long>(entries_.size())) return false;
        index_ = static_cast<std::size_t>(target);
        return true;
    }

    /// The pristine document entry's URL is the committed location, so a
    /// profile location or a replace navigation keeps it in sync. Once any
    /// entry is pushed this is a no-op (the current entry is then owned by
    /// push/replace/traversal).
    void sync_document_url(const std::string& url) {
        if (entries_.size() == 1) entries_[0].url = url;
    }

    ScrollRestoration scroll_restoration() const noexcept { return scroll_restoration_; }
    void set_scroll_restoration(ScrollRestoration value) noexcept { scroll_restoration_ = value; }

    void apply_defaults(const std::string& url = "https://example.com/") {
        entries_.clear();
        HistoryEntry entry;
        entry.id = next_id_++;
        entry.url = url;
        entry.same_document = false;
        entries_.push_back(std::move(entry));
        index_ = 0;
    }

private:
    std::vector<HistoryEntry> entries_;
    std::size_t index_ = 0;
    uint64_t next_id_ = 1;
    ScrollRestoration scroll_restoration_ = ScrollRestoration::Auto;
};

/// Entity: Performance timing information.
class PerformanceInfo {
public:
    PerformanceInfo() = default;

    double time_origin() const noexcept { return time_origin_; }
    void set_time_origin(double v) { time_origin_ = v; }

    /// performance.now() returns ms since timeOrigin
    double now_offset() const noexcept { return now_offset_; }
    void set_now_offset(double v) { now_offset_ = v; }

    void apply_defaults() {
        time_origin_ = 1700000000000.0;  // A plausible timestamp
        now_offset_ = 0.0;
    }

private:
    double time_origin_ = 1700000000000.0;
    double now_offset_ = 0.0;
};

} // namespace ruyijs::domain::browser_environment
