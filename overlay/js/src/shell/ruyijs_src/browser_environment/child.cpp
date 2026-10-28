#include "browser_environment/state.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/String.h"

namespace ruyijs::browser_environment {
Environment::Environment(JSContext* cx, JS::HandleObject page, Environment& parent,
                         JS::HandleObject frameElement)
    : state_(new State(cx, page)) {
  State* owner = parent.state_;
  state_->shared = owner->shared ? owner->shared : owner;
  state_->topWindow = owner->topWindow;
  state_->parentWindow = owner->page;
  state_->frameElement = frameElement;
}
// A later top-level document of the same traversable: a new Window over the
// first document's Model. The buffered profile groups are plain data the
// WebGL/WebGPU/Permissions/Geolocation/Speech owners of the new realm read when they bind.
Environment::Environment(JSContext* cx, JS::HandleObject page, Environment& previous,
                         Succession)
    : state_(new State(cx, page, &previous.state_->model)) {
  State& old = *previous.state_;
  state_->adopted = true;
  state_->webglGroup = old.webglGroup.get();
  state_->gpuGroup = old.gpuGroup.get();
  state_->permissionsGroup = old.permissionsGroup.get();
  state_->geolocationGroup = old.geolocationGroup.get();
  state_->speechGroup = old.speechGroup.get();
}
// The child realm (or a later top-level document) builds its own frozen
// languages array and applies the same locale and timezone to its Intl/Date;
// the values come from the one owner.
bool State::Inherit() {
  FieldValue list, language, timezone;
  const Model& source = shared ? shared->model : model;
  if (!source.Read(Kind::Navigator, 10, &list) ||
      !source.Read(Kind::Navigator, 9, &language) ||
      !source.Read(Kind::Locale, 0, &timezone)) {
    JS_ReportOutOfMemory(cx);
    return false;
  }
  JSAutoRealm realm(cx, page);
  JS::RootedObject array(cx, JS::NewArrayObject(cx, list.strings.size()));
  if (!array) return false;
  for (size_t index = 0; index < list.strings.size(); ++index) {
    const auto& source = list.strings[index];
    JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(source.data(), source.size())));
    if (!text || !JS_DefineElement(cx, array, index, text, JSPROP_ENUMERATE)) return false;
  }
  if (!JS_FreezeObject(cx, array)) return false;
  languages = array;
  JS::Realm* target = JS::GetObjectRealmOrNull(page);
  if (!language.text.empty()) JS::SetRealmLocaleOverride(target, language.text.c_str());
  if (!timezone.text.empty()) JS::SetRealmTimezoneOverride(target, timezone.text.c_str());
  ready = true;
  return true;
}
}
