#ifndef RUYIJS_ENVIRONMENT_CONTROL_H
#define RUYIJS_ENVIRONMENT_CONTROL_H
#include "js/RootingAPI.h"
#include <string>

namespace ruyijs::browser_environment { class Environment; }
namespace ruyijs {
class EnvironmentControl final {
 public:
  EnvironmentControl(JSContext* cx, browser_environment::Environment& owner)
      : cx_(cx), owner_(owner) {}
  void Freeze() { frozen_ = true; }
  bool Frozen() const { return frozen_; }
  void SetConfiguredPort(bool (*port)(void*), void* context) {
    configured_ = port; configuredContext_ = context;
  }
  bool File(const char* filename);
  bool Request(const std::string& operation, JS::HandleObject request,
               JS::MutableHandleValue result);
 private:
  JSContext* cx_;
  browser_environment::Environment& owner_;
  bool frozen_ = false;
  bool (*configured_)(void*) = nullptr;
  void* configuredContext_ = nullptr;
};
}
#endif
