#include "runtime/page_load.h"
#include "runtime/browsing_context.h"
#include "execution_scheduling/realm.h"
#include "resource_network/page_http.h"
#include "domain/browser_environment/url_value.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CallArgs.h"
#include "js/CharacterEncoding.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/String.h"

namespace ruyijs {
namespace {
enum Slot { Loader, Document, Form, Version, Generation, URL, Method, Body, ContentType, Ticket, Count };
const JSClass kFormTask = {"Form navigation task", JSCLASS_HAS_RESERVED_SLOTS(Count)};
bool Utf8(JSContext* cx, const std::u16string& value, std::string* result) {
  JS::RootedString text(cx, JS_NewUCStringCopyN(cx, value.data(), value.size()));
  return text && ToUTF8(cx, text, result);
}
std::string Lower(std::string value) {
  for (auto& c : value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
  return value;
}
bool Store(JSContext* cx, JS::HandleObject object, unsigned slot, const std::string& value) {
  JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(value.data(), value.size())));
  if (!text) return false;
  JS::SetReservedSlot(object, slot, JS::StringValue(text)); return true;
}
bool Read(JSContext* cx, JS::HandleObject object, unsigned slot, std::string* value) {
  JS::RootedString text(cx, JS::GetReservedSlot(object, slot).toString());
  return ToUTF8(cx, text, value);
}
}
bool PageLoad::FormDataPort(void* context, document_model::Realm& document,
    const std::vector<document_model::FormField>& fields, JS::MutableHandleObject result) {
  auto& loader = *static_cast<PageLoad*>(context);
  auto* target = loader.Find(document);
  if (!target) return document.ThrowException("InvalidStateError", "Form realm is retired");
  std::vector<web_platform::FormField> input;
  for (const auto& field : fields) input.push_back({field.name, field.value, field.emptyFile});
  return target->platform->CreateFormData(input, result);
}
bool PageLoad::FormNavigatePort(void* context, document_model::Realm& document,
    JS::HandleObject form, JS::HandleObject data, const document_model::FormSubmission& submission) {
  auto& loader = *static_cast<PageLoad*>(context);
  auto* cx = loader.cx_;
  auto* owner = loader.Find(document);
  if (!owner) return document.ThrowException("InvalidStateError", "Form realm is retired");
  auto& environment = *owner->environment;
  auto& platform = *owner->platform;
  std::string action, method, encoding, target, current, base, url, body, type;
  if (!Utf8(cx, submission.action, &action) || !Utf8(cx, submission.method, &method) ||
      !Utf8(cx, submission.encoding, &encoding) || !Utf8(cx, submission.target, &target)) return false;
  method = Lower(method); encoding = Lower(encoding);
  if (method == "dialog") return true;
  if (!target.empty() && Lower(target) != "_self")
    return document.ThrowException("NotSupportedError", "Form target context is not implemented");
  JS::RootedValue href(cx), reference(cx);
  if (!environment.URL(&href) || !document.ReadBaseReference(&reference)) return false;
  JS::RootedString hrefText(cx, href.toString());
  if (!ToUTF8(cx, hrefText, &current)) return false;
  base = current;
  if (reference.isString()) {
    JS::RootedString text(cx, reference.toString());
    std::string relative, resolved;
    if (!ToUTF8(cx, text, &relative)) return false;
    if (resource_network::ResolveURL(current, relative, &resolved)) base = resolved;
  }
  if (action.empty()) url = current;
  else if (!resource_network::ResolveURL(base, action, &url)) return true;
  ruyijs::domain::browser_environment::UrlValue address(url);
  if (address.protocol() != "http:" && address.protocol() != "https:") return true;
  const bool post = method == "post";
  if (!platform.EncodeFormData(data, post ? encoding : "application/x-www-form-urlencoded", &body, &type)) return false;
  if (!post) { address.set_search("?" + body); body.clear(); type.clear(); }
  JS::RootedObject task(cx, JS_NewObjectWithGivenProto(cx, &kFormTask, nullptr));
  JS::RootedFunction fn(cx, js::NewFunctionWithReserved(cx, DeliverForm, 0, 0, "Form navigation task"));
  if (!task || !fn) return false;
  JS::SetReservedSlot(task, Loader, JS::PrivateValue(&loader));
  JS::SetReservedSlot(task, Document, JS::PrivateValue(&document));
  JS::SetReservedSlot(task, Form, JS::ObjectValue(*form));
  JS::SetReservedSlot(task, Version, JS::DoubleValue(double(submission.version)));
  JS::SetReservedSlot(task, Generation, JS::DoubleValue(double(document.ScriptGeneration())));
  // A top-level plan is one more navigation of the traversable: it replaces a
  // queued Location navigation, and a later one replaces it.
  JS::SetReservedSlot(task, Ticket, JS::DoubleValue(owner->parent ? 0.0 : double(++loader.ticket_)));
  JS::RootedString bytes(cx, JS_NewStringCopyN(cx, body.data(), body.size()));
  if (!bytes) return false;
  JS::SetReservedSlot(task, Body, JS::StringValue(bytes));
  if (!Store(cx, task, URL, address.href()) || !Store(cx, task, Method, post ? "POST" : "GET") ||
      !Store(cx, task, ContentType, type)) return false;
  JS::RootedObject callback(cx, JS_GetFunctionObject(fn));
  js::SetFunctionNativeReserved(callback, 0, JS::ObjectValue(*task));
  return loader.scheduling_.PostTask(callback);
}
bool PageLoad::DeliverForm(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject task(cx, &js::GetFunctionNativeReserved(&args.callee(), 0).toObject());
  args.rval().setUndefined();
  auto& document = *static_cast<document_model::Realm*>(JS::GetReservedSlot(task, Document).toPrivate());
  if (document.ScriptGeneration() != uint64_t(JS::GetReservedSlot(task, Generation).toNumber())) return true;
  JS::RootedObject form(cx, &JS::GetReservedSlot(task, Form).toObject());
  if (!document.FormNavigationCurrent(form, uint64_t(JS::GetReservedSlot(task, Version).toNumber()))) return !JS_IsExceptionPending(cx);
  auto& loader = *static_cast<PageLoad*>(JS::GetReservedSlot(task, Loader).toPrivate());
  auto* target = loader.Find(document);
  if (!target) return true;
  const bool frame = target->parent != nullptr;
  if (!frame && loader.Started(uint64_t(JS::GetReservedSlot(task, Ticket).toNumber()))) return true;
  std::string url, method, body, type;
  JS::RootedString bytes(cx, JS::GetReservedSlot(task, Body).toString());
  JS::UniqueChars encoded = JS_EncodeStringToLatin1(cx, bytes);
  if (!encoded || !Read(cx, task, URL, &url) || !Read(cx, task, Method, &method) ||
      !Read(cx, task, ContentType, &type)) return false;
  body.assign(encoded.get(), JS_GetStringLength(bytes));
  if (frame) return loader.LoadFrame(*target, url, method, body, type);
  // The planned navigation starts here, so this is where Firefox fires
  // beforeunload at the submitting document.
  return loader.BeforeUnload() && !loader.Started(uint64_t(JS::GetReservedSlot(task, Ticket).toNumber())) ?
      loader.Navigate(url, false, method, body, type) : !JS_IsExceptionPending(cx);
}
}
