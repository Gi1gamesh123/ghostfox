// LockManager.query, the Lock attributes, steal delivery and the shared helpers.
#include "web_locks/internal.h"
#include "browser_environment/environment.h"
#include "runtime/runtime.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::web_locks {
namespace {
JSObject* Infos(State& s, const std::vector<locks::LockInfo>& infos) {
  JSContext* cx = s.cx;
  JS::RootedObject array(cx, JS::NewArrayObject(cx, 0));
  if (!array) return nullptr;
  for (size_t i = 0; i < infos.size(); ++i) {
    // LockInfo dictionary members in WebIDL (lexicographic) order.
    JS::RootedObject info(cx, JS_NewPlainObject(cx));
    JS::RootedString client(cx, JS_NewStringCopyN(cx, infos[i].client.data(), infos[i].client.size()));
    JS::RootedString mode(cx, JS_NewStringCopyZ(cx, infos[i].mode == locks::LockMode::Shared ? "shared" : "exclusive"));
    JS::RootedString name(cx, JS_NewUCStringCopyN(cx, infos[i].name.data(), infos[i].name.size()));
    if (!info || !client || !mode || !name ||
        !JS_DefineProperty(cx, info, "clientId", client, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, info, "mode", mode, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, info, "name", name, JSPROP_ENUMERATE) ||
        !JS_SetElement(cx, array, uint32_t(i), info)) return nullptr;
  }
  return array;
}
bool LockReceiver(JSContext* cx, const JS::CallArgs& args, const char* member, JS::MutableHandleObject self) {
  std::string message = std::string("'") + member + "' called on an object that does not implement interface Lock.";
  return WebKit::Check(cx, args.thisv(), &kLockClass, self, message.c_str());
}
}  // namespace

bool Query(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!WebKit::Check(cx, args.thisv(), &kManagerClass, &self,
                     "'query' called on an object that does not implement interface LockManager.")) {
    JS::RootedValue error(cx);
    if (!JS_GetPendingException(cx, &error)) return false;
    JS_ClearPendingException(cx);
    JSObject* rejected = JS::CallOriginalPromiseReject(cx, error);
    if (!rejected) return false;
    args.rval().setObject(*rejected);
    return true;
  }
  State& s = *static_cast<State*>(JS::GetReservedSlot(self, 0).toPrivate());
  std::string origin;
  if (!Origin(s, &origin)) return false;
  std::vector<locks::LockInfo> held, pending;
  s.registry->queues[origin].Query(&held, &pending);
  // LockManagerSnapshot members: held, pending.
  JS::RootedObject snapshot(cx, JS_NewPlainObject(cx));
  JS::RootedObject heldArray(cx, Infos(s, held)), pendingArray(cx, Infos(s, pending));
  if (!snapshot || !heldArray || !pendingArray ||
      !JS_DefineProperty(cx, snapshot, "held", heldArray, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, snapshot, "pending", pendingArray, JSPROP_ENUMERATE)) return false;
  JS::RootedValue value(cx, JS::ObjectValue(*snapshot));
  return s.kit.Resolve(value, args.rval());
}

bool LockName_(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!LockReceiver(cx, args, "get name", &self)) return false;
  args.rval().set(JS::GetReservedSlot(self, LockName));
  return true;
}
bool LockMode_(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!LockReceiver(cx, args, "get mode", &self)) return false;
  JSString* mode = JS_NewStringCopyZ(cx, JS::GetReservedSlot(self, LockMode).toBoolean() ? "shared" : "exclusive");
  if (!mode) return false;
  args.rval().setString(mode);
  return true;
}

// A steal releases the held locks of that name: their request promises reject
// with AbortError (LockRequestChild::Recv__delete__); the callback's own result
// still settles later but no longer owns a lock.
bool Deliver(Registry& registry, const locks::LockQueue::Step& step) {
  for (const auto& entry : step.stolen) {
    auto found = registry.owners.find(entry.id);
    if (found == registry.owners.end()) continue;
    State& s = *found->second;
    JSAutoRealm realm(s.cx, s.kit.page);
    JS::RootedValue value(s.cx), promise(s.cx), error(s.cx);
    if (!JS_GetProperty(s.cx, s.requests, std::to_string(entry.id).c_str(), &value)) return false;
    registry.owners.erase(found);
    if (!value.isObject()) continue;
    JS::RootedObject record(s.cx, &value.toObject());
    if (!JS_GetElement(s.cx, record, ReqPromise, &promise) ||
        !JS_DeleteProperty(s.cx, s.requests, std::to_string(entry.id).c_str()) ||
        !Exception(s, "AbortError", "The lock request is aborted", &error)) return false;
    JS::RootedObject request(s.cx, &promise.toObject());
    if (!JS::RejectPromise(s.cx, request, error)) return false;
  }
  return DeliverGranted(registry, step.granted);
}

bool Origin(State& state, std::string* origin) {
  JS::RootedValue value(state.cx);
  if (!state.environment.Origin(&value)) return false;
  if (!value.isString()) { *origin = "null:" + state.client; return true; }
  JS::RootedString text(state.cx, value.toString());
  if (!ToUTF8(state.cx, text, origin)) return false;
  if (*origin == "null") *origin += ":" + state.client;
  return true;
}

bool Exception(State& state, const char* name, const char* message, JS::MutableHandleValue out) {
  JSContext* cx = state.cx;
  JS::RootedValue constructor(cx, JS::ObjectValue(*state.exception));
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, message)), kind(cx, JS_NewStringCopyZ(cx, name));
  JS::RootedValueArray<2> values(cx);
  JS::RootedObject error(cx);
  if (!text || !kind) return false;
  values[0].setString(text);
  values[1].setString(kind);
  if (!JS::Construct(cx, constructor, values, &error)) return false;
  out.setObject(*error);
  return true;
}
}  // namespace ruyijs::web_locks
