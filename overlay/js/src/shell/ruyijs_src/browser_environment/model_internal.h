#ifndef RUYIJS_BROWSER_ENVIRONMENT_MODEL_INTERNAL_H
#define RUYIJS_BROWSER_ENVIRONMENT_MODEL_INTERNAL_H
#include "browser_environment/model.h"
#include "domain/browser_environment/browser_environment.h"
#include "domain/browser_environment/touch_exposure.h"
namespace ruyijs::browser_environment {
struct Model::Impl {
  ruyijs::domain::browser_environment::BrowserEnvironmentAggregate aggregate{1};
  // Window touch-interface exposure input. The root Environment adapter samples
  // the platform once; Navigator's reported maxTouchPoints remains independent.
  bool touch_platform_supported = false;
  ruyijs::domain::browser_environment::TouchEventsOverride touch_override =
      ruyijs::domain::browser_environment::TouchEventsOverride::Auto;
  // The Firefox-only navigator traits (kNavigatorFields 15..20). The imported
  // ruyijs NavigatorInfo has no such fields, so ruyijs keeps them beside the
  // aggregate instead of editing the byte-identical domain import. Defaults
  // match profiles/firefox-environment.json; the baseline profile always
  // overwrites them before any page can read them.
  std::string oscpu;
  std::string build_id;
  std::string do_not_track;
  bool pdf_viewer_enabled = true;
  bool online = true;
  bool global_privacy_control = false;
  // kScreenFields 8/9 (screen.top / screen.left). One primary screen sits at the
  // origin, so the default is 0/0; profiles/firefox-environment.json sets them
  // through the same validated screen group as every other Screen field.
  int screen_top = 0;
  int screen_left = 0;
  // -1 preserves the legacy work-area default until explicitly configured.
  int viewport_width = -1;
  int viewport_height = -1;
  // kViewportFields 2 (viewport.devicePixelRatio). Firefox's own default on a
  // 100% scaling desktop display is 1; the CSS `resolution` media features
  // derive from this one value (web_styling reads it through Describe()).
  double device_pixel_ratio = 1.0;
  // kStorageFields (quota / usage / persisted). The quota is Firefox's desktop
  // default for a fresh origin; the headless runtime keeps no persistent store,
  // so usage and persistence read the configured values only.
  double storage_quota = 2147483648.0;
  double storage_usage = 0.0;
  bool storage_persisted = false;
};
}
#endif
