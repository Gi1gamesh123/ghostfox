#include "browser_environment/model_internal.h"
#include <type_traits>
#include <utility>

namespace ruyijs::browser_environment {
static_assert(std::is_nothrow_move_assignable_v<OriginValue>);
bool Model::ReadOriginValue(OriginValue* result) const noexcept {
  if (!impl_) return false;
  try {
    auto value = impl_->aggregate.location().url().origin_value();
    *result = std::move(value); return true;
  } catch (...) { return false; }
}
bool Model::ReadUrlOrigin(const std::string& source, OriginValue* result) noexcept {
  try {
    auto value = domain::browser_environment::UrlValue(source).origin_value();
    *result = std::move(value); return true;
  } catch (...) { return false; }
}
bool Model::CopyOriginValue(const OriginValue& source, OriginValue* result) noexcept {
  try {
    auto value = source;
    *result = std::move(value); return true;
  } catch (...) { return false; }
}
Model::~Model() noexcept { delete impl_; }
bool Model::Initialize() noexcept {
  if (impl_) return true;
  try { impl_ = new Impl; return true; }
  catch (...) { return false; }
}
bool Model::SetPlatformTouchSupport(bool supported) noexcept {
  if (!impl_) return false;
  impl_->touch_platform_supported = supported;
  return true;
}
bool Model::TouchEventsEnabled(bool* result) const noexcept {
  if (!impl_) return false;
  *result = ruyijs::domain::browser_environment::ResolveTouchEvents(
      impl_->touch_override, impl_->touch_platform_supported);
  return true;
}

bool Model::Read(Kind kind, uint32_t field, FieldValue* result) const noexcept {
  if (!impl_) return false;
  try {
    const auto& owner = impl_->aggregate;
    if (kind == Kind::Navigator) {
      const auto& value = owner.navigator();
      switch (field) {
        case 0: result->text = value.user_agent(); break;
        case 1: result->text = value.platform(); break;
        case 2: result->text = value.app_code_name(); break;
        case 3: result->text = value.app_name(); break;
        case 4: result->text = value.app_version(); break;
        case 5: result->text = value.product(); break;
        case 6: result->text = value.product_sub(); break;
        case 7: result->text = value.vendor(); break;
        case 8: result->text = value.vendor_sub(); break;
        case 9: result->text = value.language(); break;
        case 10: result->strings = value.languages(); break;
        case 11: result->integer = value.hardware_concurrency(); break;
        case 12: result->integer = value.max_touch_points(); break;
        case 13: result->boolean = value.cookie_enabled(); break;
        case 14: result->boolean = value.webdriver(); break;
        // Firefox-only traits kept in Model::Impl; see model_internal.h.
        case 15: result->text = impl_->oscpu; break;
        case 16: result->text = impl_->build_id; break;
        case 17: result->text = impl_->do_not_track; break;
        case 18: result->boolean = impl_->pdf_viewer_enabled; break;
        case 19: result->boolean = impl_->online; break;
        case 20: result->boolean = impl_->global_privacy_control; break;
        default: return false;
      }
    } else if (kind == Kind::Screen) {
      const auto& value = owner.screen();
      switch (field) {
        case 0: result->integer = value.width(); break;
        case 1: result->integer = value.height(); break;
        case 2: result->integer = value.avail_width(); break;
        case 3: result->integer = value.avail_height(); break;
        case 4: result->integer = value.avail_left(); break;
        case 5: result->integer = value.avail_top(); break;
        case 6: result->integer = value.color_depth(); break;
        case 7: result->integer = value.pixel_depth(); break;
        // Firefox-only CSSOM-View origin fields kept in Model::Impl.
        case 8: result->integer = impl_->screen_top; break;
        case 9: result->integer = impl_->screen_left; break;
        default: return false;
      }
    } else if (kind == Kind::Location) {
      const auto& value = owner.location();
      switch (field) {
        case 0: result->text = value.href(); break;
        case 1: result->text = value.protocol(); break;
        case 2: result->text = value.host(); break;
        case 3: result->text = value.hostname(); break;
        case 4: result->text = value.port(); break;
        case 5: result->text = value.pathname(); break;
        case 6: result->text = value.search(); break;
        case 7: result->text = value.hash(); break;
        case 8: result->text = value.origin(); break;
        default: return false;
      }
    } else if (kind == Kind::History && field == 0) {
      result->integer = owner.history().length();
    } else if (kind == Kind::Speech) {
      // The baseline voice list of this environment (kSpeechFields order):
      // names, one language per name, then the default voice name.
      const auto& value = owner.speech();
      if (field == 0) {
        for (const auto& voice : value.voices()) result->strings.push_back(voice.name);
      } else if (field == 1) {
        for (const auto& voice : value.voices()) result->strings.push_back(voice.lang);
      } else if (field == 2) {
        result->text = value.default_voice();
      } else return false;
    } else if (kind == Kind::Storage) {
      // The StorageManager owner's estimate and persistence answer.
      if (field == 0) result->number = impl_->storage_quota;
      else if (field == 1) result->number = impl_->storage_usage;
      else if (field == 2) result->boolean = impl_->storage_persisted;
      else return false;
    } else if (kind == Kind::Locale && field == 0) {
      result->text = owner.locale().timezone();
    } else if (kind == Kind::Viewport) {
      if (field == 0) result->integer = impl_->viewport_width < 0
          ? owner.screen().avail_width() : impl_->viewport_width;
      else if (field == 1) result->integer = impl_->viewport_height < 0
          ? owner.screen().avail_height() : impl_->viewport_height;
      else if (field == 2) result->number = impl_->device_pixel_ratio;
      else return false;
    } else return false;
    return true;
  } catch (...) { return false; }
}
}
