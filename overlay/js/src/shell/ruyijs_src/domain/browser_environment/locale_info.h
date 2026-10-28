#pragma once

#include <string>
#include <utility>

namespace ruyijs::domain::browser_environment {

// Value object for the context-scoped timezone observed by Date and Intl.
class LocaleInfo {
public:
    const std::string& timezone() const noexcept { return timezone_; }
    int timezone_offset() const noexcept { return timezone_offset_; }
    // The legacy offset is a source sample, never a rule for Date/Intl or DST.
    bool has_timezone_offset_sample() const noexcept { return has_offset_sample_; }

    bool configure_timezone(std::string timezone) {
        if (timezone.empty()) return false;
        timezone_ = std::move(timezone);
        has_offset_sample_ = false;
        return true;
    }

    bool configure(std::string timezone, int timezone_offset) {
        if (timezone.empty() || timezone_offset < -840 || timezone_offset > 840)
            return false;
        timezone_ = std::move(timezone);
        timezone_offset_ = timezone_offset;
        has_offset_sample_ = true;
        return true;
    }

private:
    std::string timezone_ = "America/New_York";
    int timezone_offset_ = 300;
    bool has_offset_sample_ = true;
};

} // namespace ruyijs::domain::browser_environment
