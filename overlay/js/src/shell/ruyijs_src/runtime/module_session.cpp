#include "runtime/module_session.h"

#include "resource_network/page_http.h"
#include "resource_network/model.h"
#include "runtime/runtime.h"
#include "jsapi.h"
#include "js/CharacterEncoding.h"
#include "js/CompileOptions.h"
#include "js/Modules.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/SourceText.h"
#include "js/StableStringChars.h"
#include "js/String.h"

namespace ruyijs {
namespace {
const JSErrorFormatString* TypeErrorFormat(void*, unsigned) {
  static const JSErrorFormatString format{"RUYIJS_MODULE", "{0}", 1, JSEXN_TYPEERR};
  return &format;
}
bool TypeError(JSContext* cx, const std::string& message) {
  JS_ReportErrorNumberUTF8(cx, TypeErrorFormat, nullptr, 0, message.c_str());
  return false;
}
}  // namespace

thread_local ModuleSession* ModuleSession::current_ = nullptr;
thread_local std::vector<ModuleSession*> ModuleSession::live_;

ModuleSession::Scope::Scope(ModuleSession& session) noexcept
    : session_(&session), previous_(ModuleSession::current_) {
  ModuleSession::current_ = session_;
}
ModuleSession::Scope::~Scope() noexcept { ModuleSession::current_ = previous_; }

ModuleSession* ModuleSession::Current(JSContext* cx) noexcept {
  if (current_ && current_->cx_ == cx) return current_;
  // HostLoadImportedModule for a dynamic import() runs in the realm of the
  // script that called it; that realm's newest live generation owns the map.
  JSObject* global = JS::CurrentGlobalOrNull(cx);
  if (!global) return nullptr;
  for (auto it = live_.rbegin(); it != live_.rend(); ++it)
    if ((*it)->cx_ == cx && !(*it)->retired_ && (*it)->global_.get() == global) return *it;
  return nullptr;
}

bool ModuleSession::EnsureHooks(JSRuntime* runtime) {
  // The embedding has one pair of module hooks per JSRuntime.  Install the
  // same adapter every time a document session is created instead of caching
  // a raw runtime address: runtimes can be created concurrently and an address
  // can be reused after shutdown.
  JS::SetModuleLoadHook(runtime, &ModuleSession::LoadImportedModule);
  JS::SetModuleMetadataHook(runtime, &ModuleSession::PopulateImportMeta);
  return true;
}

ModuleSession::ModuleSession(JSContext* cx, resource_network::Model& resources)
    : cx_(cx), resources_(resources), global_(cx, JS::CurrentGlobalOrNull(cx)),
      hostDefined_(cx, nullptr), root_(cx, nullptr) {
  live_.push_back(this);
  Scope scope(*this);
  JS::RootedObject marker(cx_, JS_NewPlainObject(cx_));
  if (marker) hostDefined_ = marker;
  EnsureHooks(JS_GetRuntime(cx_));
}

ModuleSession::~ModuleSession() noexcept {
  if (current_ == this) current_ = nullptr;
  for (auto it = live_.begin(); it != live_.end(); ++it)
    if (*it == this) { live_.erase(it); break; }
  records_.clear();
  root_ = nullptr;
  hostDefined_ = nullptr;
}

bool ModuleSession::ToUtf8(JS::HandleString value, std::string* output) {
  return ruyijs::ToUTF8(cx_, value, output);
}

JSObject* ModuleSession::Find(const std::string& url) const noexcept {
  auto it = records_.find(url);
  return it == records_.end() ? nullptr : it->second.module->get();
}

bool ModuleSession::SetPrivateUrl(JS::HandleObject module, const std::string& url) {
  JS::RootedString value(cx_, JS_NewStringCopyUTF8N(
      cx_, JS::UTF8Chars(url.data(), url.size())));
  if (!value) return false;
  JS::SetModulePrivate(module, JS::StringValue(value));
  return true;
}

bool ModuleSession::FetchAndCompile(const std::string& url,
                                    JS::MutableHandleObject module) {
  resource_network::HttpResult http;
  resource_network::HttpOptions options;
  options.initiator = "script";
  if (!resource_network::PerformHttp(resources_, "GET", url, {}, "", &http,
                                     resource_network::RedirectMode::Follow,
                                     options) || http.status < 200 || http.status > 299) {
    if (dynamic_) return TypeError(cx_, "error loading dynamically imported module: " + url);
    JS_ReportErrorASCII(cx_, "Module request failed: %s", url.c_str());
    return false;
  }
  size_t bom = http.body.rfind("\xEF\xBB\xBF", 0) == 0 ? 3 : 0;
  auto* bytes = reinterpret_cast<const mozilla::Utf8Unit*>(http.body.data() + bom);
  JS::SourceText<mozilla::Utf8Unit> source;
  if (!source.init(cx_, bytes, http.body.size() - bom,
                   JS::SourceOwnership::Borrowed)) return false;
  JS::CompileOptions optionsCompile(cx_);
  optionsCompile.setFileAndLine(url.c_str(), 1);
  JS::RootedObject compiled(cx_, JS::CompileModule(cx_, optionsCompile, source));
  if (!compiled || !SetPrivateUrl(compiled, url)) return false;
  Record record;
  record.url = url;
  record.module = std::make_unique<JS::PersistentRootedObject>(cx_, compiled);
  records_.emplace(url, std::move(record));
  module.set(compiled);
  return true;
}

bool ModuleSession::ResolveImport(JS::HandleValue referrer,
                                  JS::HandleObject request,
                                  JS::MutableHandleObject module, bool* fresh) {
  if (fresh) *fresh = false;
  // A module referrer carries its URL; a classic script, an event handler or
  // eval code resolves against the document's base URL (HTML "script base URL"
  // for inline and evaluated code).
  JS::RootedValue privateValue(cx_, referrer.isUndefined() || referrer.isNull()
                                        ? JS::UndefinedValue()
                                        : JS::GetReferrerPrivate(referrer));
  std::string baseText, specifierText, url;
  if (privateValue.isString()) {
    JS::RootedString base(cx_, privateValue.toString());
    if (!ToUtf8(base, &baseText)) return false;
  } else if (!DocumentBase(&baseText)) {
    return false;
  }
  JS::RootedString specifier(cx_, JS::GetModuleRequestSpecifier(cx_, request));
  if (!specifier || !ToUtf8(specifier, &specifierText)) return false;
  // HTML "resolve a module specifier" without an import map (Firefox's text).
  const bool relative = specifierText.rfind("/", 0) == 0 || specifierText.rfind("./", 0) == 0 ||
                        specifierText.rfind("../", 0) == 0;
  if (!relative && specifierText.find(':') == std::string::npos)
    return TypeError(cx_, "The specifier “" + specifierText +
                              "” was a bare specifier, but was not remapped to anything. "
                              "Relative module specifiers must start with “./”, "
                              "“../” or “/”.");
  if (!resource_network::ResolveURL(baseText, specifierText, &url)) {
    JS_ReportErrorASCII(cx_, "Invalid module URL");
    return false;
  }
  if (JSObject* found = Find(url)) {
    module.set(found);
    return true;
  }
  if (fresh) *fresh = true;
  return FetchAndCompile(url, module);
}

bool ModuleSession::DocumentBase(std::string* base) {
  if (!global_) {
    JS_ReportErrorASCII(cx_, "Module referrer has no URL");
    return false;
  }
  JS::RootedObject global(cx_, global_);
  JSAutoRealm realm(cx_, global);
  JS::RootedValue document(cx_), uri(cx_);
  if (!JS_GetProperty(cx_, global, "document", &document) || !document.isObject()) return false;
  JS::RootedObject documentObject(cx_, &document.toObject());
  if (!JS_GetProperty(cx_, documentObject, "baseURI", &uri) || !uri.isString()) {
    if (!JS_IsExceptionPending(cx_)) JS_ReportErrorASCII(cx_, "Module referrer has no URL");
    return false;
  }
  JS::RootedString text(cx_, uri.toString());
  return ToUtf8(text, base);
}

bool ModuleSession::Load(const std::string& url, const std::string& source,
                         JS::MutableHandleObject module) {
  Scope scope(*this);
  if (!hostDefined_) return false;
  if (JSObject* found = Find(url)) {
    module.set(found);
  } else {
    JS::RootedString bytes(cx_, JS_NewStringCopyUTF8N(
        cx_, JS::UTF8Chars(source.data(), source.size())));
    if (!bytes) return false;
    JS::AutoStableStringChars chars(cx_);
    if (!chars.initTwoByte(cx_, bytes)) return false;
    JS::SourceText<char16_t> text;
    if (!text.initMaybeBorrowed(cx_, chars)) return false;
    JS::CompileOptions options(cx_);
    options.setFileAndLine(url.c_str(), 1);
    JS::RootedObject compiled(cx_, JS::CompileModule(cx_, options, text));
    if (!compiled || !SetPrivateUrl(compiled, url)) return false;
    Record record;
    record.url = url;
    record.module = std::make_unique<JS::PersistentRootedObject>(cx_, compiled);
    records_.emplace(url, std::move(record));
    module.set(compiled);
  }
  root_ = module;
  JS::RootedValue host(cx_, JS::ObjectValue(*hostDefined_));
  if (!JS::LoadRequestedModules(cx_, module, host, &ModuleSession::LinkLoaded,
                                &ModuleSession::LinkRejected)) return false;
  // LinkRejected reports the graph error through the pending exception while
  // the callback API itself returns success.  Keep that failure visible to
  // PageLoad so it can dispatch the page error and continue the script queue.
  if (JS_IsExceptionPending(cx_)) return false;
  return true;
}

bool ModuleSession::Evaluate(JS::HandleObject module, JS::MutableHandleValue promise) {
  Scope scope(*this);
  return JS::ModuleEvaluate(cx_, module, promise);
}

bool ModuleSession::LoadImportedModule(JSContext* cx, JS::HandleValue referrer,
                                       JS::HandleObject request, JS::HandleValue,
                                       JS::HandleValue payload, uint32_t,
                                       JS::ColumnNumberOneOrigin) {
  ModuleSession* session = Current(cx);
  if (!session) { JS_ReportErrorASCII(cx, "Module session is not active"); return false; }
  // ModuleLoaderBase::HostLoadImportedModule: a promise payload is import().
  JS::RootedObject payloadObject(cx, payload.isObject() ? &payload.toObject() : nullptr);
  if (payloadObject && JS::IsPromiseObject(payloadObject))
    return session->LoadDynamic(referrer, request, payload);
  JS::RootedObject module(cx);
  if (!session->ResolveImport(referrer, request, &module)) return false;
  return JS::FinishLoadingImportedModule(cx, referrer, request, payload, module, true);
}

bool ModuleSession::LoadDynamic(JS::HandleValue referrer, JS::HandleObject request,
                                JS::HandleValue payload) {
  JS::RootedObject module(cx_);
  bool fresh = false;
  dynamic_ = true;
  const bool resolved = ResolveImport(referrer, request, &module, &fresh);
  dynamic_ = false;
  if (!resolved)
    return JS::FinishLoadingImportedModuleFailedWithPendingException(cx_, payload);
  if (!fresh)
    return JS::FinishLoadingImportedModule(cx_, referrer, request, payload, module, true);
  // The completion record travels as LoadRequestedModules' hostDefined value.
  JS::RootedObject record(cx_, JS_NewPlainObject(cx_));
  JS::RootedValue requestValue(cx_, JS::ObjectValue(*request));
  JS::RootedValue moduleValue(cx_, JS::ObjectValue(*module));
  if (!record || !JS_DefineProperty(cx_, record, "referrer", referrer, 0) ||
      !JS_DefineProperty(cx_, record, "request", requestValue, 0) ||
      !JS_DefineProperty(cx_, record, "payload", payload, 0) ||
      !JS_DefineProperty(cx_, record, "module", moduleValue, 0))
    return false;
  JS::RootedValue host(cx_, JS::ObjectValue(*record));
  return JS::LoadRequestedModules(cx_, module, host, &ModuleSession::DynamicLoaded,
                                  &ModuleSession::DynamicRejected);
}

bool ModuleSession::DynamicLoaded(JSContext* cx, JS::HandleValue host) {
  JS::RootedObject record(cx, &host.toObject());
  JS::RootedValue referrer(cx), request(cx), payload(cx), module(cx);
  if (!JS_GetProperty(cx, record, "referrer", &referrer) ||
      !JS_GetProperty(cx, record, "request", &request) ||
      !JS_GetProperty(cx, record, "payload", &payload) ||
      !JS_GetProperty(cx, record, "module", &module)) return false;
  JS::RootedObject requestObject(cx, &request.toObject()), moduleObject(cx, &module.toObject());
  return JS::FinishLoadingImportedModule(cx, referrer, requestObject, payload, moduleObject, true);
}

bool ModuleSession::DynamicRejected(JSContext* cx, JS::HandleValue host, JS::HandleValue error) {
  JS::RootedObject record(cx, &host.toObject());
  JS::RootedValue payload(cx);
  if (!JS_GetProperty(cx, record, "payload", &payload)) return false;
  return JS::FinishLoadingImportedModuleFailed(cx, payload, error);
}

bool ModuleSession::PopulateImportMeta(JSContext* cx, JS::HandleObject module,
                                       JS::HandleObject meta) {
  JS::RootedValue value(cx, JS::GetModulePrivate(module));
  if (!value.isString()) return true;
  return JS_DefineProperty(cx, meta, "url", value, JSPROP_ENUMERATE);
}

bool ModuleSession::LinkLoaded(JSContext* cx, JS::HandleValue) {
  ModuleSession* session = Current(cx);
  return session && session->root_ && JS::ModuleLink(cx, session->root_);
}

bool ModuleSession::LinkRejected(JSContext* cx, JS::HandleValue, JS::HandleValue error) {
  JS_SetPendingException(cx, error);
  return true;
}
}  // namespace ruyijs
