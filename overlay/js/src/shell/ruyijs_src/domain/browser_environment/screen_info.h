#pragma once

#include <cstdint>
#include <string>

namespace ruyijs::domain::browser_environment {

/// Entity: Screen information (display capabilities).
class ScreenInfo {
public:
    ScreenInfo() = default;

    int width() const noexcept { return width_; }
    void set_width(int v) { width_ = v; }

    int height() const noexcept { return height_; }
    void set_height(int v) { height_ = v; }

    int avail_width() const noexcept { return avail_width_; }
    void set_avail_width(int v) { avail_width_ = v; }

    int avail_height() const noexcept { return avail_height_; }
    void set_avail_height(int v) { avail_height_ = v; }

    int color_depth() const noexcept { return color_depth_; }
    void set_color_depth(int v) { color_depth_ = v; }

    int pixel_depth() const noexcept { return pixel_depth_; }
    void set_pixel_depth(int v) { pixel_depth_ = v; }

    int avail_left() const noexcept { return avail_left_; }
    void set_avail_left(int v) { avail_left_ = v; }

    int avail_top() const noexcept { return avail_top_; }
    void set_avail_top(int v) { avail_top_ = v; }

    const std::string& orientation_type() const noexcept { return orientation_type_; }
    void set_orientation_type(std::string v) { orientation_type_ = std::move(v); }
    int orientation_angle() const noexcept { return orientation_angle_; }
    void set_orientation_angle(int v) noexcept { orientation_angle_ = v; }

    /// Apply Chrome/Windows 1080p defaults
    void apply_defaults() {
        width_ = 1920;
        height_ = 1080;
        avail_width_ = 1920;
        avail_height_ = 1040;  // Taskbar excluded
        color_depth_ = 24;
        pixel_depth_ = 24;
        avail_left_ = 0;
        avail_top_ = 0;
        orientation_type_ = "landscape-primary";
        orientation_angle_ = 0;
    }

private:
    int width_ = 1920;
    int height_ = 1080;
    int avail_width_ = 1920;
    int avail_height_ = 1040;
    int color_depth_ = 24;
    int pixel_depth_ = 24;
    int avail_left_ = 0;
    int avail_top_ = 0;
    std::string orientation_type_ = "landscape-primary";
    int orientation_angle_ = 0;
};

} // namespace ruyijs::domain::browser_environment
