#pragma once

#include "domain/shared/aggregate_root.h"
#include "domain/browser_environment/navigator_info.h"
#include "domain/browser_environment/screen_info.h"
#include "domain/browser_environment/location_info.h"
#include "domain/browser_environment/locale_info.h"
#include "domain/browser_environment/speech_info.h"
#include "domain/browser_environment/fingerprint_config.h"
#include "domain/browser_environment/fingerprint_merge_service.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace ruyijs::domain::browser_environment {

using BrowserEnvId = uint64_t;

/// Aggregate Root: BrowserEnvironment
/// Manages the complete browser fingerprint state for a single context.
/// Owns Navigator, Screen, Location, History, Performance entities.
/// All modifications go through this aggregate root.
class BrowserEnvironmentAggregate : public shared::AggregateRoot<BrowserEnvId> {
public:
    explicit BrowserEnvironmentAggregate(BrowserEnvId id)
        : AggregateRoot<BrowserEnvId>(id) {
        initialize_with_defaults();
    }

    /// Initialize with default Chrome/Windows fingerprint
    void initialize_with_defaults() {
        config_ = FingerprintMergeService::build_defaults();
        FingerprintMergeService::apply_to_navigator(config_, navigator_);
        FingerprintMergeService::apply_to_screen(config_, screen_);
        FingerprintMergeService::apply_to_location(config_, location_);
        history_.apply_defaults(location_.href());
        performance_.apply_defaults();
        speech_.apply_defaults();
    }

    /// Apply user-provided environment overrides
    void apply_overrides(const FingerprintConfig& overrides) {
        config_ = FingerprintMergeService::merge(config_, overrides);
        FingerprintMergeService::apply_to_navigator(config_, navigator_);
        FingerprintMergeService::apply_to_screen(config_, screen_);
        FingerprintMergeService::apply_to_location(config_, location_);
        history_.sync_document_url(location_.href());
    }

    // --- Entity accessors (read-only from outside aggregate) ---

    const NavigatorInfo& navigator() const noexcept { return navigator_; }
    const ScreenInfo& screen() const noexcept { return screen_; }
    const LocationInfo& location() const noexcept { return location_; }
    const HistoryInfo& history() const noexcept { return history_; }
    const SpeechInfo& speech() const noexcept { return speech_; }
    const PerformanceInfo& performance() const noexcept { return performance_; }
    const LocaleInfo& locale() const noexcept { return locale_; }
    const FingerprintConfig& config() const noexcept { return config_; }

    // --- Operations that modify internal state ---

    /// Navigate to a new URL (updates location + pushes history)
    void navigate_to(const std::string& url) {
        location_.navigate(url);
        HistoryEntry entry;
        entry.url = url;
        history_.push_state(std::move(entry));
    }

    /// pushState: same-document URL change; commits one new entry and updates
    /// the location without any navigation. `state` is the opaque
    /// structured-clone blob produced by the engine adapter.
    void push_history(std::string url, std::vector<uint8_t> state, std::vector<uint8_t> navigation = {}) {
        location_.navigate(url);
        HistoryEntry entry;
        entry.url = std::move(url);
        entry.same_document = true;
        entry.state = std::move(state);
        entry.navigation_state = std::move(navigation);
        history_.push_state(std::move(entry));
    }

    /// replaceState: replaces the current entry and the URL without changing
    /// the history length.
    void replace_history(std::string url, std::vector<uint8_t> state, std::vector<uint8_t> navigation = {}) {
        location_.navigate(url);
        HistoryEntry entry;
        entry.url = std::move(url);
        entry.same_document = true;
        entry.state = std::move(state);
        entry.navigation_state = std::move(navigation);
        history_.replace_state(std::move(entry));
    }

    /// A replace navigation commit: replaces the current entry with a new
    /// document entry (location.replace / reload), keeping the length.
    void replace_navigation(const std::string& url) {
        location_.navigate(url);
        HistoryEntry entry;
        entry.url = url;
        history_.replace_state(std::move(entry));
    }

    /// scrollRestoration: the one flag on the session-history owner. Firefox's
    /// default is "auto"; "manual" is the only other value the IDL accepts.
    void set_history_scroll_restoration(bool manual) {
        history_.set_scroll_restoration(manual
            ? ScrollRestoration::Manual : ScrollRestoration::Auto);
    }

    /// Traverses the session history; returns whether the current entry moved.
    void set_navigation_state(std::vector<uint8_t> state) { history_.set_navigation_state(std::move(state)); }

    bool go_history(int delta) {
        if (!history_.go(delta)) return false;
        location_.navigate(history_.current().url);
        return true;
    }

    void configure_navigator(NavigatorInfo navigator) {
        navigator_ = std::move(navigator);
    }

    void configure_screen(ScreenInfo screen) {
        screen_ = std::move(screen);
    }

    void configure_location(const std::string& url) {
        location_.navigate(url);
        history_.sync_document_url(url);
    }

    void configure_performance_time_origin(double value) {
        performance_.set_time_origin(value);
    }

    bool configure_locale(std::string timezone, int timezone_offset) {
        return locale_.configure(std::move(timezone), timezone_offset);
    }

    bool configure_locale_timezone(std::string timezone) {
        return locale_.configure_timezone(std::move(timezone));
    }

    /// Update performance.now() offset
    void advance_performance_now(double ms) {
        performance_.set_now_offset(performance_.now_offset() + ms);
    }

    /// Get a config value by dot-path (for get_defaults() API)
    std::string get_config_value(const std::string& path) const {
        return config_.get(path);
    }

private:
    FingerprintConfig config_;
    NavigatorInfo navigator_;
    ScreenInfo screen_;
    LocationInfo location_;
    HistoryInfo history_;
    SpeechInfo speech_;
    PerformanceInfo performance_;
    LocaleInfo locale_;
};

} // namespace ruyijs::domain::browser_environment
