// The Response constructor, the Response.* static methods and the interface
// installer. The object layout is owned by response_object.cpp; this file only
// normalizes arguments into it.
#include "resource_network/state.h"
#include "resource_network/body.h"
#include "resource_network/page_http.h"
#include "runtime/page_helpers.h"
#include "runtime/native_constructor_trace.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"
#include "js/JSON.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "js/Symbol.h"
#include <memory>
#include <new>

namespace ruyijs::resource_network {
namespace {

bool ResponseCreate(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  State& state = *FunctionOwner(args);
  if (!args.isConstructing()) return Error(cx, "Response requires new");
  state.Observe("Response", "constructor", int(ApiAccess::Construct));
  JS::RootedObject object(cx);
  if (!CreateResponse(state, args, 0, "default", HeaderPairs(), nullptr, &object)) return false;
  args.rval().setObject(*object);
  return trace.Finish(true);
}

bool Serialize(JSContext* cx, JS::HandleValue data, std::string* body) {
  std::u16string text;
  JS::RootedValue value(cx, data);
  JS::RootedObject replacer(cx);
  JS::RootedValue space(cx, JS::UndefinedValue());
  if (!JS_Stringify(cx, &value, replacer, space,
                    [](const char16_t* data, uint32_t length, void* context) {
                      static_cast<std::u16string*>(context)->append(data, length);
                      return true;
                    },
                    &text)) return false;
  return Report(cx, ByteString(text.data(), text.size(), body));
}

// Response.json(data, init): the body is the JSON text and content-type
// defaults to application/json unless init.headers already carries one.
bool JsonCall(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionOwner(args);
  state.Observe("Response", "json", int(ApiAccess::Call));
  std::string body;
  if (!Serialize(cx, args.get(0), &body)) return false;
  HeaderPairs headers;
  bool given_headers = false;
  if (args.get(1).isObject()) {
    JS::RootedObject init(cx, &args[1].toObject());
    JS::RootedValue value(cx);
    if (!JS_GetProperty(cx, init, "headers", &value)) return false;
    if (!value.isUndefined()) {
      given_headers = true;
      if (!HeaderPairsArgument(cx, value, &headers)) return false;
    }
  }
  bool content_type = false;
  for (const auto& field : headers) content_type = content_type || field.first == "content-type";
  if (!content_type)
    headers.emplace_back(std::string("content-type"), std::string("application/json"));
  (void)given_headers;
  JS::RootedObject object(cx);
  if (!CreateResponse(state, args, 200, "default", headers, body.c_str(), &object)) return false;
  args.rval().setObject(*object);
  return true;
}

// Response.error(): the network error response, type "error" and no body.
bool ErrorCall(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionOwner(args);
  state.Observe("Response", "error", int(ApiAccess::Call));
  JS::RootedObject headers(cx);
  JS::RootedString status_text(cx, JS_NewStringCopyZ(cx, ""));
  JS::RootedString url(cx, JS_NewStringCopyZ(cx, ""));
  std::unique_ptr<BodyRecord> body(new (std::nothrow) BodyRecord());
  JS::RootedObject object(cx);
  if (!status_text || !url || !body) return false;
  if (!HeaderObject(state, HeaderPairs(), &headers, true) ||
      !ResponseNew(state, state.responsePrototype, body.get(), headers, 0, status_text, url,
                   "error", false, &object)) return false;
  body.release();
  args.rval().setObject(*object);
  return true;
}

// Response.redirect(url, status): the redirect statuses only, Location set.
bool RedirectCall(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionOwner(args);
  state.Observe("Response", "redirect", int(ApiAccess::Call));
  std::string input, base, url;
  if (!Convert(cx, args.get(0), &input) || !PageBaseURL(state, &base) ||
      !ResolveURL(base, input, &url)) return Error(cx, "Response.redirect requires a URL");
  int32_t status = 302;
  if (!args.get(1).isUndefined()) {
    double number = 0;
    if (!JS::ToNumber(cx, args.get(1), &number)) return false;
    status = int32_t(number);
  }
  if (status != 301 && status != 302 && status != 303 && status != 307 && status != 308)
    return Error(cx, "Invalid redirect status", true);
  HeaderPairs headers{std::make_pair(std::string("location"), url)};
  JS::RootedObject object(cx);
  if (!CreateResponse(state, args, status, "default", headers, nullptr, &object)) return false;
  args.rval().setObject(*object);
  return true;
}

bool InstallStatics(State& state, PageHelpers& helpers, JS::HandleObject constructor) {
  JSContext* cx = state.cx;
  struct Static { const char* name; JSNative native; unsigned length; };
  const Static statics[] = {{"json", JsonCall, 1}, {"error", ErrorCall, 0},
                            {"redirect", RedirectCall, 1}};
  for (const Static& entry : statics) {
    JS::RootedObject function(cx, NativeFunction(state, entry.native, entry.name, entry.length));
    if (!function || !JS_DefineProperty(cx, constructor, entry.name, function, JSPROP_ENUMERATE) ||
        !helpers.Member(function, "Response", entry.name, "call")) return false;
  }
  return true;
}
}  // namespace

bool InstallResponse(State& state, PageHelpers& helpers) {
  JSContext* cx = state.cx;
  JS::RootedObject prototype(cx, JS_NewPlainObject(cx));
  JS::RootedObject constructor(cx,
                                  NativeFunction(state, ResponseCreate, "Response", 0,
                                                  JSFUN_CONSTRUCTOR));
  if (!prototype || !constructor ||
      !JS_DefineProperty(cx, constructor, "prototype", prototype,
                         JSPROP_READONLY | JSPROP_PERMANENT) ||
      !JS_DefineProperty(cx, prototype, "constructor", constructor, 0) ||
      !Tag(cx, prototype, "Response")) return false;
  state.responsePrototype = prototype;
  return InstallResponseObject(state, helpers, prototype) &&
         InstallStatics(state, helpers, constructor) &&
         helpers.Constructor("Response", constructor) &&
         JS_DefineProperty(cx, state.page, "Response", constructor, 0);
}
}  // namespace ruyijs::resource_network
