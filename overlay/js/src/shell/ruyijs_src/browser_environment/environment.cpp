#include "browser_environment/external.h"
#include "browser_environment/navigation_api.h"
#include "browser_environment/window_legacy.h"
#include "browser_environment/browser_chrome.h"
#include "browser_environment/window_handlers.h"
#include "browser_environment/plugins.h"
#include "browser_environment/screen_orientation.h"
#include "browser_environment/storage_manager.h"
#include "browser_environment/visual_viewport.h"
#include "browser_environment/state.h"
#include "browser_environment/string_list.h"
#include "browser_environment/touch_platform.h"
#include "interaction/realm.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/String.h"

#include "browser_environment/cross_origin.h"
namespace ruyijs::browser_environment {
State::~State() {
  UnregisterWindow(this);
  for (JSObject* function : functions.get())
    js::SetFunctionNativeReserved(function, 0, JS::PrivateValue(nullptr));
  delete opfsStore;
  opfsStore = nullptr;
}
Environment::Environment(JSContext* cx, JS::HandleObject page)
    : state_(new State(cx, page)) {}
Environment::~Environment() { delete state_; }
bool Environment::WindowView(JSContext* cx, JS::HandleObject target, JS::MutableHandleValue out) {
  return browser_environment::WindowView(cx, nullptr, target, out);
}
bool Environment::SameOriginWithCurrent(JSContext* cx, JS::HandleObject target, bool* same) {
  if (HostCaller(cx)) return *same = true, true;
  return SameOriginWindows(cx, JS::CurrentGlobalOrNull(cx), target, same);
}
bool Environment::TouchCapable() const { return state_->TouchCapable(); }
bool Environment::Prepare() { return state_->Baseline(); }
bool Environment::TouchEventsEnabled(bool* result) {
  return Prepare() && state_->TouchEventsEnabled(result);
}

bool State::Apply(JS::HandleObject profile, bool complete) {
  std::vector<Change> changes;
  return Profile(cx, profile, complete, &changes) && ApplyChanges(changes);
}
bool State::ApplyChanges(std::vector<Change>& changes) {
  if (!ValidateLocale(&changes)) return false;
  // The webgl group is the WebGL owner's data, not the environment model's:
  // route it to the graphics port and keep only the other kinds here.
  std::vector<Change> modelChanges;
  bool webgl = false, webgpu = false, permissions = false, geolocation = false, speech = false,
       media = false;
  bool owners[kOwnerGroupCount] = {};
  for (const auto& change : changes) {
    if (change.kind == Kind::WebGL) { webgl = true; continue; }
    if (change.kind == Kind::WebGpu) { webgpu = true; continue; }
    if (change.kind == Kind::Permissions) { permissions = true; continue; }
    if (change.kind == Kind::Geolocation) { geolocation = true; continue; }
    if (change.kind == Kind::Speech) { speech = true; continue; }
    if (change.kind == Kind::Media) { media = true; continue; }
    size_t owner = 0;
    while (owner < kOwnerGroupCount && kOwnerGroups[owner].kind != change.kind) ++owner;
    if (owner < kOwnerGroupCount) { owners[owner] = true; continue; }
    modelChanges.push_back(change);
  }
  JS::RootedObject nextLanguages(cx, languages);
  for (const auto& change : modelChanges) {
    if (change.kind != Kind::Navigator || change.field != 10) continue;
    JSAutoRealm realm(cx, page);
    nextLanguages = JS::NewArrayObject(cx, change.value.strings.size());
    if (!nextLanguages) return false;
    for (size_t index = 0; index < change.value.strings.size(); ++index) {
      const auto& source = change.value.strings[index];
      JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx,
          JS::UTF8Chars(source.data(), source.size())));
      if (!text || !JS_DefineElement(cx, nextLanguages, index, text, JSPROP_ENUMERATE))
        return false;
    }
    if (!JS_FreezeObject(cx, nextLanguages)) return false;
  }
  if (!model.Initialize() || !model.Configure(modelChanges)) {
    JS_ReportOutOfMemory(cx);
    return false;
  }
  languages = nextLanguages;
  ApplyLocale(modelChanges);
  if (webgl && (!MergeWebGL(changes) || !PushWebGL())) return false;
  if (webgpu && (!MergeWebGpu(changes) || !PushWebGpu())) return false;
  if (permissions && (!MergePermissions(changes) || !PushPermissions())) return false;
  if (geolocation && (!MergeGeolocation(changes) || !PushGeolocation())) return false;
  if (speech && (!MergeSpeech(changes) || !PushSpeech())) return false;
  if (media && (!MergeMedia(changes) || !PushMedia())) return false;
  for (size_t owner = 0; owner < kOwnerGroupCount; ++owner)
    if (owners[owner] && (!MergeOwner(owner, changes) || !PushOwner(owner))) return false;
  return true;
}
bool State::MergeWebGL(const std::vector<Change>& changes) {
  JSAutoRealm realm(cx, page);
  JS::RootedObject buffer(cx, webglGroup);
  if (!MergeGroup(cx, &buffer, Kind::WebGL, kWebGLFields, std::size(kWebGLFields),
                  changes)) return false;
  webglGroup = buffer;
  return true;
}
bool State::PushWebGL() {
  if (!webglGroup.get() || !graphicsPort) return true;
  JSAutoRealm realm(cx, page);
  JS::RootedObject group(cx, webglGroup);
  return graphicsPort(graphicsPortContext, group);
}
bool State::MergeWebGpu(const std::vector<Change>& changes) {
  JSAutoRealm realm(cx, page);
  JS::RootedObject buffer(cx, gpuGroup);
  if (!MergeGroup(cx, &buffer, Kind::WebGpu, kWebGpuFields, std::size(kWebGpuFields),
                  changes)) return false;
  gpuGroup = buffer;
  return true;
}
bool State::MergePermissions(const std::vector<Change>& changes) {
  JSAutoRealm realm(cx, page);
  if (!permissionsGroup.get()) {
    JS::RootedObject group(cx, JS_NewPlainObject(cx));
    if (!group) return false;
    permissionsGroup = group;
  }
  for (const auto& change : changes) {
    if (change.kind != Kind::Permissions) continue;
    JS::RootedString text(cx, JS_NewStringCopyN(cx, change.value.text.data(),
                                                change.value.text.size()));
    if (!text || !JS_DefineProperty(cx, permissionsGroup, kPermissionsFields[change.field].name,
                                    text, JSPROP_ENUMERATE)) return false;
  }
  return true;
}
bool State::PushPermissions() {
  if (!permissionsGroup.get() || !permissionsPort) return true;
  JSAutoRealm realm(cx, page);
  JS::RootedObject group(cx, permissionsGroup);
  return permissionsPort(permissionsPortContext, group);
}
bool State::MergeGeolocation(const std::vector<Change>& changes) {
  JSAutoRealm realm(cx, page);
  if (!geolocationGroup.get()) {
    JS::RootedObject group(cx, JS_NewPlainObject(cx));
    if (!group) return false;
    geolocationGroup = group;
  }
  for (const auto& change : changes) {
    if (change.kind != Kind::Geolocation) continue;
    JS::RootedValue value(cx, change.value.null ? JS::NullValue()
                                                : JS::DoubleValue(change.value.number));
    if (!JS_DefineProperty(cx, geolocationGroup, kGeolocationFields[change.field].name,
                           value, JSPROP_ENUMERATE)) return false;
  }
  return true;
}
bool State::PushGeolocation() {
  if (!geolocationGroup.get() || !geolocationPort) return true;
  JSAutoRealm realm(cx, page);
  JS::RootedObject group(cx, geolocationGroup);
  return geolocationPort(geolocationPortContext, group);
}
bool State::MergeSpeech(const std::vector<Change>& changes) {
  JSAutoRealm realm(cx, page);
  if (!speechGroup.get()) {
    JS::RootedObject group(cx, JS_NewPlainObject(cx));
    if (!group) return false;
    speechGroup = group;
  }
  for (const auto& change : changes) {
    if (change.kind != Kind::Speech) continue;
    JS::RootedValue value(cx);
    if (kSpeechFields[change.field].type == ValueType::String) {
      JSString* text = JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(change.value.text.data(),
                                                               change.value.text.size()));
      if (!text) return false;
      value.setString(text);
    } else {
      JS::RootedObject list(cx, JS::NewArrayObject(cx, change.value.strings.size()));
      if (!list) return false;
      for (size_t index = 0; index < change.value.strings.size(); ++index) {
        const auto& source = change.value.strings[index];
        JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(source.data(), source.size())));
        if (!text || !JS_DefineElement(cx, list, index, text, JSPROP_ENUMERATE)) return false;
      }
      value.setObject(*list);
    }
    if (!JS_DefineProperty(cx, speechGroup, kSpeechFields[change.field].name, value,
                           JSPROP_ENUMERATE)) return false;
  }
  return true;
}
// One string array property of the speech group; the group's own shape is
// the schema's kSpeechFields, so a profile group and this baseline agree.
bool SpeechStrings(JSContext* cx, const std::vector<std::string>& sources,
                   JS::HandleObject group, const char* name) {
  JS::RootedObject list(cx, JS::NewArrayObject(cx, sources.size()));
  if (!list) return false;
  for (size_t index = 0; index < sources.size(); ++index) {
    JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx,
        JS::UTF8Chars(sources[index].data(), sources[index].size())));
    if (!text || !JS_DefineElement(cx, list, index, text, JSPROP_ENUMERATE)) return false;
  }
  return JS_DefineProperty(cx, group, name, list, JSPROP_ENUMERATE);
}
bool State::SpeechBaseline(JS::MutableHandleObject result) {
  // The environment model's voice list as a "speech" group object, built from
  // the same kSpeechFields shape a profile group uses.
  JSAutoRealm realm(cx, page);
  FieldValue names, langs, name;
  JS::RootedObject group(cx, JS_NewPlainObject(cx));
  if (!group || !model.Read(Kind::Speech, 0, &names) ||
      !model.Read(Kind::Speech, 1, &langs) || !model.Read(Kind::Speech, 2, &name) ||
      !SpeechStrings(cx, names.strings, group, kSpeechFields[0].name) ||
      !SpeechStrings(cx, langs.strings, group, kSpeechFields[1].name)) return false;
  JS::RootedString defaultName(cx, JS_NewStringCopyUTF8N(cx,
      JS::UTF8Chars(name.text.data(), name.text.size())));
  JS::RootedValue value(cx, JS::StringValue(defaultName));
  if (!defaultName || !JS_DefineProperty(cx, group, kSpeechFields[2].name, value,
                                          JSPROP_ENUMERATE)) return false;
  result.set(group);
  return true;
}
bool State::EffectiveSpeech(JS::MutableHandleObject result) {
  // The one reading of this context's effective voice list: the configured
  // group when one exists, otherwise the model baseline. Pushing it to the
  // owner and reporting it through Describe() share this construction.
  if (speechGroup.get()) {
    result.set(speechGroup);
    return true;
  }
  // An iframe's own model holds no profile (it reads its creator's state, as
  // Navigator and Screen do), so its voice list is the creator's effective one,
  // wrapped into this realm.
  if (shared) {
    JS::RootedObject group(cx);
    if (!shared->EffectiveSpeech(&group)) return false;
    JSAutoRealm realm(cx, page);
    if (!JS_WrapObject(cx, &group)) return false;
    result.set(group);
    return true;
  }
  return SpeechBaseline(result);
}
bool State::PushSpeech() {
  // No profile "speech" group was configured for this context, so the owner
  // receives the environment model's baseline voice list through the same
  // port: one data path for profile voices and for the baseline ones.
  if (!speechGroup.get() && (!speechPort || !ready)) return true;
  JSAutoRealm realm(cx, page);
  JS::RootedObject group(cx);
  if (!EffectiveSpeech(&group)) return false;
  if (!speechGroup.get()) speechGroup = group;
  if (!speechPort) return true;
  return speechPort(speechPortContext, group);
}
bool State::MergeMedia(const std::vector<Change>& changes) {
  JSAutoRealm realm(cx, page);
  JS::RootedObject buffer(cx, mediaGroup);
  if (!MergeGroup(cx, &buffer, Kind::Media, kMediaFields, std::size(kMediaFields),
                  changes)) return false;
  mediaGroup = buffer;
  return true;
}
bool State::PushMedia() {
  if (!mediaGroup.get() || !mediaPort) return true;
  JSAutoRealm realm(cx, page);
  JS::RootedObject group(cx, mediaGroup);
  return mediaPort(mediaPortContext, group);
}
bool State::PushWebGpu() {
  if (!gpuGroup.get() || !gpuPort) return true;
  JSAutoRealm realm(cx, page);
  JS::RootedObject group(cx, gpuGroup);
  return gpuPort(gpuPortContext, group);
}
bool State::Baseline() {
  if (ready) return true;
  if (shared || adopted) return Inherit();
  JS::RootedValue value(cx);
  if (!ParseBaseline(cx, &value) || !value.isObject()) return false;
  JS::RootedObject profile(cx, &value.toObject());
  if (!Apply(profile, true)) return false;
  if (!worker && !model.SetPlatformTouchSupport(PlatformSupportsTouchEvents())) {
    JS_ReportOutOfMemory(cx);
    return false;
  }
  ready = true;
  return true;
}
bool Environment::Configure(JS::HandleObject profile) {
  return state_->Baseline() && state_->Apply(profile, false);
}
bool Environment::Defaults(JS::MutableHandleValue result) {
  return state_->Baseline() && ParseBaseline(state_->cx, result);
}
bool Environment::Install(interaction::Realm& events, PageHelpers& helpers) {
  State& state = *state_;
  if (state.installed) return Error(state.cx, "Environment is already installed");
  state.helpers = &helpers;
  if (!state.Baseline()) return false;
  JSAutoRealm realm(state.cx, state.page);
  JS::RootedObject objectPrototype(state.cx, JS::GetRealmObjectPrototype(state.cx));
  JS::RootedObject eventPrototype(state.cx, events.prototype());
  if (!objectPrototype || !eventPrototype) return false;
  // The DOMStringList interface lands before the instances: Location's
  // ancestorOrigins accessor creates instances of it.
  if (!InstallStringList(state)) return false;
  for (Kind kind : {Kind::Navigator, Kind::Screen, Kind::Location, Kind::History}) {
    JS::RootedObject parent(state.cx, kind == Kind::Screen ? eventPrototype : objectPrototype);
    if (!state.Instance(kind, parent)) return false;
  }
  // The PDF plugin surface and the one External instance are realm-local and
  // must exist before the Window facade exposes window.external.
  if (!InstallPluginSurface(state) || !InstallExternalSurface(state)) return false;
  JS::RootedObject screen(state.cx, state.instances.get()[int32_t(Kind::Screen)]);
  // Screen's own orientation members and the realm-local ScreenOrientation
  // instance, then the VisualViewport instance window.visualViewport returns.
  // Both must exist before the Window facade installs its member table.
  if (!InstallScreenOrientation(state, events) || !InstallScreenMembers(state) ||
      !InstallVisualViewport(state, events)) return false;
  if (!events.Attach(screen) || !state.Globals() || !state.Window(eventPrototype) ||
      !state.WindowMembers() || !state.NavigatorPlatformMembers() ||
      !state.NavigatorLegacyMembers() ||
      !InstallStorageManager(state) ||
      !InstallBrowserChrome(state) || !InstallWindowHandlers(state, events) ||
      !InstallNavigation(state, events) || !InstallWindowLegacy(state, events)) return false;
  state.installed = true;
  return true;
}
}
