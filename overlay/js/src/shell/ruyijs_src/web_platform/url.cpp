// The URL interface. The object keeps only its serialized href and reads every
// component back through the domain's single parser (UrlValue); setters write a
// new serialization and keep a live URLSearchParams object in step, so the two
// views of the query never disagree.
#include "web_platform/state.h"
#include "domain/browser_environment/url_value.h"
#include "runtime/web_kit.h"
#include "runtime/native_constructor_trace.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::web_platform {
namespace {
using ruyijs::domain::browser_environment::UrlValue;

bool Current(State& state, JS::HandleObject self, UrlValue* url) {
  JSContext* cx = state.cx;
  JS::RootedValue slot(cx, JS::GetReservedSlot(self, kUrlHrefSlot));
  std::string href;
  if (!slot.isString() || !Convert(cx, slot, &href))
    return WebKit::Fail(cx, "URL state is not a serialized href");
  *url = UrlValue(href);
  return true;
}

// Keeps the cached URLSearchParams object consistent with a new href.
bool ResyncParams(State& state, JS::HandleObject self, const UrlValue& url) {
  JSContext* cx = state.cx;
  JS::RootedValue cached(cx, JS::GetReservedSlot(self, kUrlParamsSlot));
  if (!cached.isObject()) return true;
  JS::RootedObject params(cx, &cached.toObject());
  std::string search = url.search();
  JS::RootedValue query(cx);
  if (!Value(cx, search.empty() ? "" : search.substr(1), &query)) return false;
  JS::SetReservedSlot(params, kParamsQuerySlot, query);
  return true;
}

bool Store(State& state, JS::HandleObject self, const UrlValue& url) {
  JSContext* cx = state.cx;
  JS::RootedValue text(cx);
  if (!Value(cx, url.href(), &text)) return false;
  JS::SetReservedSlot(self, kUrlHrefSlot, text);
  return ResyncParams(state, self, url);
}

// The (url, base) argument pair shared by the constructor, URL.parse and
// URL.canParse. A present base is parsed first and on its own, so an empty or
// invalid base fails even for an absolute input; |invalid| is the string the
// donor names in its TypeError. Returns false only for a pending exception.
bool ParseArguments(JSContext* cx, const JS::CallArgs& args, UrlValue* url, bool* ok,
                    std::string* invalid) {
  std::string input, base;
  if (!Convert(cx, args.get(0), &input)) return false;
  bool has_base = args.length() > 1 && !args.get(1).isUndefined();
  if (has_base && !Convert(cx, args.get(1), &base)) return false;
  UrlValue parsed_base;
  if (has_base && !UrlValue::Resolve(base, "", &parsed_base)) {
    *ok = false;
    *invalid = base;
    return true;
  }
  *ok = UrlValue::Resolve(input, has_base ? parsed_base.href() : "", url);
  *invalid = input;
  return true;
}

bool NewUrl(State& state, JS::HandleObject prototype, const UrlValue& url,
            JS::MutableHandleObject object) {
  JSContext* cx = state.cx;
  object.set(state.kit.New(&kUrlClass, prototype));
  JS::RootedValue text(cx);
  if (!object || !Value(cx, url.href(), &text)) return false;
  JS::SetReservedSlot(object, kUrlHrefSlot, text);
  JS::SetReservedSlot(object, kUrlParamsSlot, JS::UndefinedValue());
  return true;
}

bool MissingArgument(JSContext* cx, const char* prefix) {
  return WebKit::Fail(cx, (std::string(prefix) +
                           ": At least 1 argument required, but only 0 passed").c_str());
}
}  // namespace

bool UrlConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  NativeConstructorTrace trace(cx, args);
  if (!args.isConstructing()) return WebKit::Fail(cx, "URL constructor: 'new' is required");
  if (!argc) return MissingArgument(cx, "URL constructor");
  UrlValue url;
  bool ok = false;
  std::string invalid;
  if (!ParseArguments(cx, args, &url, &ok, &invalid)) return false;
  if (!ok)
    return WebKit::Fail(cx, ("URL constructor: " + invalid + " is not a valid URL.").c_str());
  JS::RootedObject prototype(cx);
  if (!state.kit.ConstructProto(args, state.url.prototype, &prototype)) return false;
  JS::RootedObject object(cx);
  if (!NewUrl(state, prototype, url, &object)) return false;
  args.rval().setObject(*object);
  return trace.Finish(true);
}

// URL.parse: the constructor's parse without the throw. The result always
// takes this realm's URL.prototype; there is no newTarget.
bool UrlParse(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  if (!argc) return MissingArgument(cx, "URL.parse");
  UrlValue url;
  bool ok = false;
  std::string invalid;
  if (!ParseArguments(cx, args, &url, &ok, &invalid)) return false;
  if (!ok) {
    args.rval().setNull();
    return true;
  }
  JS::RootedObject object(cx);
  if (!NewUrl(state, state.url.prototype, url, &object)) return false;
  args.rval().setObject(*object);
  return true;
}

bool UrlCanParse(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!argc) return MissingArgument(cx, "URL.canParse");
  UrlValue url;
  bool ok = false;
  std::string invalid;
  if (!ParseArguments(cx, args, &url, &ok, &invalid)) return false;
  args.rval().setBoolean(ok);
  return true;
}

bool UrlGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  if (!WebKit::CheckBrand(cx, args, &kUrlClass, &self, "URL")) return false;
  UrlValue url;
  if (!Current(state, self, &url)) return false;
  switch (UrlField(WebKit::Slot(args))) {
    case kHref: return Value(cx, url.href(), args.rval());
    case kProtocol: return Value(cx, url.protocol(), args.rval());
    case kHost: return Value(cx, url.host(), args.rval());
    case kHostname: return Value(cx, url.hostname(), args.rval());
    case kPort: return Value(cx, url.port(), args.rval());
    case kPathname: return Value(cx, url.pathname(), args.rval());
    case kSearch: return Value(cx, url.search(), args.rval());
    case kHash: return Value(cx, url.hash(), args.rval());
    case kUsername: return Value(cx, url.username(), args.rval());
    case kPassword: return Value(cx, url.password(), args.rval());
    case kOrigin: {
      std::string origin = url.origin();
      return Value(cx, origin, args.rval());
    }
    case kSearchParams: {
      JS::RootedValue cached(cx, JS::GetReservedSlot(self, kUrlParamsSlot));
      if (cached.isObject()) { args.rval().set(cached); return true; }
      std::string search = url.search();
      JS::RootedObject params(cx, NewSearchParams(
          state, search.empty() ? "" : search.substr(1), self));
      if (!params) return false;
      JS::SetReservedSlot(self, kUrlParamsSlot, JS::ObjectValue(*params));
      args.rval().setObject(*params);
      return true;
    }
  }
  return WebKit::Fail(cx, "Unknown URL member");
}

bool UrlSetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  if (!WebKit::CheckBrand(cx, args, &kUrlClass, &self, "URL")) return false;
  UrlValue url;
  if (!Current(state, self, &url)) return false;
  std::string text;
  if (!Convert(cx, args.get(0), &text)) return false;
  switch (UrlField(WebKit::Slot(args))) {
    case kHref: {
      UrlValue next;
      if (!UrlValue::Resolve(text, "", &next)) return WebKit::Fail(cx, "Invalid URL");
      return Store(state, self, next);
    }
    case kProtocol: url.set_protocol(text); break;
    case kHost: url.set_host(text); break;
    case kHostname: url.set_hostname(text); break;
    case kPort: url.set_port(text); break;
    case kPathname: url.set_pathname(text); break;
    case kSearch: url.set_search(text); break;
    case kHash: url.set_hash(text); break;
    case kUsername: url.set_username(text); break;
    case kPassword: url.set_password(text); break;
    default: return WebKit::Fail(cx, "URL member is not writable");
  }
  args.rval().setUndefined();
  return Store(state, self, url);
}

}  // namespace ruyijs::web_platform
