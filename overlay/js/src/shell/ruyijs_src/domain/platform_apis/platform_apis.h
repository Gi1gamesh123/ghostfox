#pragma once

#include "domain/shared/aggregate_root.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ruyijs::domain::platform_apis {

/// Value Object: WebGL parameters (from environment.webgl.* config).
struct GlParameters {
    std::string vendor = "Google Inc. (NVIDIA)";
    std::string renderer = "ANGLE (NVIDIA, NVIDIA GeForce GTX 1080 Direct3D11 vs_5_0 ps_5_0)";
    std::string version = "WebGL 1.0 (OpenGL ES 2.0 Chromium)";
    std::string shading_language_version = "WebGL GLSL ES 1.0 (OpenGL ES GLSL ES 1.0 Chromium)";
    int max_texture_size = 16384;
    int max_renderbuffer_size = 16384;
    int max_viewport_dims_width = 16384;
    int max_viewport_dims_height = 16384;
    int max_vertex_attribs = 16;
    int max_texture_image_units = 16;
    float max_texture_max_anisotropy = 16.0f;
    std::vector<std::string> supported_extensions = {
        "ANGLE_instanced_arrays", "EXT_blend_minmax", "EXT_color_buffer_half_float",
        "EXT_float_blend", "EXT_frag_depth", "EXT_shader_texture_lod",
        "EXT_texture_filter_anisotropic", "OES_element_index_uint",
        "OES_standard_derivatives", "OES_texture_float", "OES_texture_float_linear",
        "OES_texture_half_float", "OES_texture_half_float_linear",
        "OES_vertex_array_object", "WEBGL_color_buffer_float",
        "WEBGL_compressed_texture_s3tc", "WEBGL_debug_renderer_info",
        "WEBGL_debug_shaders", "WEBGL_depth_texture", "WEBGL_draw_buffers",
        "WEBGL_lose_context", "WEBGL_multi_draw",
    };
};

/// Value Object: Storage entry (key-value pair).
struct StorageEntry {
    std::string key;
    std::string value;
};

/// Entity: Web Storage (localStorage / sessionStorage).
/// In-memory implementation at domain level.
class WebStorage {
public:
    WebStorage() = default;

    void set_item(const std::string& key, const std::string& value) {
        if (!has_item(key)) key_order_.push_back(key);
        data_[key] = value;
    }

    std::string get_item(const std::string& key) const {
        auto it = data_.find(key);
        return (it != data_.end()) ? it->second : "";
    }

    bool has_item(const std::string& key) const {
        return data_.count(key) > 0;
    }

    void remove_item(const std::string& key) {
        data_.erase(key);
        key_order_.erase(
            std::remove(key_order_.begin(), key_order_.end(), key), key_order_.end());
    }

    void clear() {
        data_.clear();
        key_order_.clear();
    }

    size_t length() const noexcept { return data_.size(); }

    std::string key_at(size_t index) const {
        return index < key_order_.size() ? key_order_[index] : "";
    }

    const std::map<std::string, std::string>& all() const noexcept { return data_; }

private:
    std::map<std::string, std::string> data_;
    std::vector<std::string> key_order_;
};

/// Aggregate Root: PlatformAPIs context.
/// Groups Canvas, WebGL, Audio, Storage, Worker stubs.
/// Most are interface-level stubs that return plausible default values.
class PlatformApisAggregate : public shared::AggregateRoot<uint64_t> {
public:
    explicit PlatformApisAggregate(uint64_t id = 1)
        : AggregateRoot<uint64_t>(id) {}

    // --- Storage ---
    WebStorage& local_storage() noexcept { return local_storage_; }
    WebStorage& session_storage() noexcept { return session_storage_; }
    const WebStorage& local_storage() const noexcept { return local_storage_; }
    const WebStorage& session_storage() const noexcept { return session_storage_; }

    // --- WebGL parameters ---
    GlParameters& gl_params() noexcept { return gl_params_; }
    const GlParameters& gl_params() const noexcept { return gl_params_; }

    void set_gl_params(GlParameters params) { gl_params_ = std::move(params); }

    // --- Canvas stub state ---
    int canvas_width() const noexcept { return canvas_width_; }
    int canvas_height() const noexcept { return canvas_height_; }
    void set_canvas_size(int w, int h) { canvas_width_ = w; canvas_height_ = h; }

    /// toDataURL returns a minimal valid data URI (1x1 transparent PNG)
    static std::string default_data_url() {
        return "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==";
    }

    // --- Cookie store ---
    const std::string& cookie_string() const noexcept { return cookie_string_; }
    void set_cookie_string(std::string c) { cookie_string_ = std::move(c); }

    /// Parse and append a Set-Cookie value
    void append_cookie(const std::string& cookie) {
        if (cookie_string_.empty()) {
            cookie_string_ = cookie;
        } else {
            cookie_string_ += "; " + cookie;
        }
    }

private:
    WebStorage local_storage_;
    WebStorage session_storage_;
    GlParameters gl_params_;
    int canvas_width_ = 300;
    int canvas_height_ = 150;
    std::string cookie_string_;
};

} // namespace ruyijs::domain::platform_apis
