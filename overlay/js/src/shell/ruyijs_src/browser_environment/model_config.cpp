#include "browser_environment/model_internal.h"
#include <memory>

namespace ruyijs::browser_environment {
bool Model::Configure(const std::vector<Change>& changes) noexcept {
  if (!impl_) return false;
  try {
    auto candidate = std::make_unique<Impl>(*impl_);
    auto navigator = candidate->aggregate.navigator();
    auto screen = candidate->aggregate.screen();
    std::string extra_oscpu = candidate->oscpu;
    std::string extra_build_id = candidate->build_id;
    std::string extra_do_not_track = candidate->do_not_track;
    bool extra_pdf_viewer = candidate->pdf_viewer_enabled;
    bool extra_online = candidate->online;
    int extra_screen_top = candidate->screen_top;
    int extra_screen_left = candidate->screen_left;
    int extra_viewport_width = candidate->viewport_width;
    int extra_viewport_height = candidate->viewport_height;
    double extra_device_pixel_ratio = candidate->device_pixel_ratio;
    double extra_storage_quota = candidate->storage_quota;
    double extra_storage_usage = candidate->storage_usage;
    bool extra_storage_persisted = candidate->storage_persisted;
    for (const auto& change : changes) {
      const auto& value = change.value;
      if (change.kind == Kind::Navigator) {
        switch (change.field) {
          case 0: navigator.set_user_agent(value.text); break;
          case 1: navigator.set_platform(value.text); break;
          case 2: navigator.set_app_code_name(value.text); break;
          case 3: navigator.set_app_name(value.text); break;
          case 4: navigator.set_app_version(value.text); break;
          case 5: navigator.set_product(value.text); break;
          case 6: navigator.set_product_sub(value.text); break;
          case 7: navigator.set_vendor(value.text); break;
          case 8: navigator.set_vendor_sub(value.text); break;
          case 9: navigator.set_language(value.text); break;
          case 10: navigator.set_languages(value.strings); break;
          case 11: navigator.set_hardware_concurrency(value.integer); break;
          case 12: navigator.set_max_touch_points(value.integer); break;
          case 13: navigator.set_cookie_enabled(value.boolean); break;
          case 14: navigator.set_webdriver(value.boolean); break;
          // Firefox-only traits kept in Model::Impl; see model_internal.h.
          case 15: extra_oscpu = value.text; break;
          case 16: extra_build_id = value.text; break;
          case 17: extra_do_not_track = value.text; break;
          case 18: extra_pdf_viewer = value.boolean; break;
          case 19: extra_online = value.boolean; break;
          case 20: candidate->global_privacy_control = value.boolean; break;
          default: return false;
        }
      } else if (change.kind == Kind::Screen) {
        switch (change.field) {
          case 0: screen.set_width(value.integer); break;
          case 1: screen.set_height(value.integer); break;
          case 2: screen.set_avail_width(value.integer); break;
          case 3: screen.set_avail_height(value.integer); break;
          case 4: screen.set_avail_left(value.integer); break;
          case 5: screen.set_avail_top(value.integer); break;
          case 6: screen.set_color_depth(value.integer); break;
          case 7: screen.set_pixel_depth(value.integer); break;
          // Firefox-only CSSOM-View origin fields kept in Model::Impl.
          case 8: extra_screen_top = value.integer; break;
          case 9: extra_screen_left = value.integer; break;
          default: return false;
        }
      } else if (change.kind == Kind::Location && change.field == 0) {
        candidate->aggregate.configure_location(value.text);
      } else if (change.kind == Kind::Locale && change.field == 0) {
        if (!candidate->aggregate.configure_locale_timezone(value.text)) return false;
      } else if (change.kind == Kind::Storage) {
        switch (change.field) {
          case 0: extra_storage_quota = value.number; break;
          case 1: extra_storage_usage = value.number; break;
          case 2: extra_storage_persisted = value.boolean; break;
          default: return false;
        }
      } else if (change.kind == Kind::Viewport) {
        if (change.field == 0) extra_viewport_width = value.integer;
        else if (change.field == 1) extra_viewport_height = value.integer;
        else if (change.field == 2) extra_device_pixel_ratio = value.number;
        else return false;
      } else return false;
    }
    candidate->oscpu = std::move(extra_oscpu);
    candidate->build_id = std::move(extra_build_id);
    candidate->do_not_track = std::move(extra_do_not_track);
    candidate->pdf_viewer_enabled = extra_pdf_viewer;
    candidate->online = extra_online;
    candidate->screen_top = extra_screen_top;
    candidate->screen_left = extra_screen_left;
    candidate->viewport_width = extra_viewport_width;
    candidate->viewport_height = extra_viewport_height;
    candidate->device_pixel_ratio = extra_device_pixel_ratio;
    candidate->storage_quota = extra_storage_quota;
    candidate->storage_usage = extra_storage_usage;
    candidate->storage_persisted = extra_storage_persisted;
    candidate->aggregate.configure_navigator(std::move(navigator));
    candidate->aggregate.configure_screen(std::move(screen));
    delete impl_;
    impl_ = candidate.release();
    return true;
  } catch (...) { return false; }
}
}
