#include "runtime/broadcast_ports.h"
#include "web_broadcast/realm.h"
#include "web_workers/realm.h"
#include "web_workers/child.h"
#include "web_workers/clone.h"
#include "jsapi.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include <string>

namespace ruyijs {
namespace {
constexpr char kTag = '\x07';
void Put(std::string& out, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) out.push_back(char(value >> (8 * i)));
}
bool Get(std::string_view frame, size_t at, uint32_t* value) {
  if (frame.size() < at + 4) return false;
  *value = 0;
  for (unsigned i = 0; i < 4; ++i) *value |= uint32_t(static_cast<unsigned char>(frame[at + i])) << (8 * i);
  return true;
}
bool Exception(JSContext* cx, JS::HandleObject page, JS::MutableHandleObject prototype) {
  JS::RootedValue constructor(cx), value(cx);
  if (!JS_GetProperty(cx, page, "DOMException", &constructor) || !constructor.isObject()) return false;
  JS::RootedObject object(cx, &constructor.toObject());
  if (!JS_GetProperty(cx, object, "prototype", &value) || !value.isObject()) return false;
  prototype.set(&value.toObject());
  return true;
}
bool Encode(BroadcastRoute& r, const std::string& origin, const std::u16string& name, JS::HandleValue data,
            std::string* frame) {
  // The sender's realm is current: its DOMException reports a clone failure.
  JS::RootedObject exception(r.cx), page(r.cx, JS::CurrentGlobalOrNull(r.cx));
  std::string clone;
  if (!page || !Exception(r.cx, page, &exception) ||
      !web_workers::EncodeMessage(r.cx, exception, data, JS::UndefinedHandleValue, &clone)) return false;
  frame->assign(1, kTag);
  Put(*frame, uint32_t(origin.size()));
  frame->append(origin);
  Put(*frame, uint32_t(name.size()));
  for (char16_t unit : name) { frame->push_back(char(unit & 255)); frame->push_back(char(unit >> 8)); }
  frame->append(clone);
  return true;
}
// Delivers a relayed frame to this process's channels.
bool Deliver(BroadcastRoute& r, std::string_view frame) {
  uint32_t origin = 0, units = 0;
  if (frame.empty() || frame[0] != kTag || !Get(frame, 1, &origin) || frame.size() < 5 + origin ||
      !Get(frame, 5 + origin, &units) || frame.size() < 9 + origin + size_t(units) * 2) return true;
  std::u16string name;
  for (uint32_t i = 0; i < units; ++i) {
    const size_t at = 9 + origin + size_t(i) * 2;
    name.push_back(char16_t(static_cast<unsigned char>(frame[at]) | (uint16_t(static_cast<unsigned char>(frame[at + 1])) << 8)));
  }
  JSAutoRealm realm(r.cx, r.page);
  JS::RootedValue data(r.cx);
  if (!web_workers::DecodeMessage(r.cx, frame.substr(9 + origin + size_t(units) * 2), &data)) {
    JS_ClearPendingException(r.cx);  // a message this realm cannot read is dropped
    return true;
  }
  return r.channels.Receive(std::string(frame.substr(5, origin)), name, data);
}
}  // namespace

void BindBroadcastRoute(BroadcastRoute& route) {
  // A message posted here: to every worker process of the group, and up to the parent.
  route.channels.SetRelayPort([](void* context, const std::string& origin, const std::u16string& name,
                                 JS::HandleValue data) {
    auto& r = *static_cast<BroadcastRoute*>(context);
    if ((!r.children || !r.children->HasBroadcastTargets()) && !r.parent) return true;
    std::string frame;
    if (!Encode(r, origin, name, data, &frame)) return false;
    if (r.children && !r.children->Broadcast(frame, nullptr)) return false;
    return !r.parent || r.parent->Queue(std::move(frame));
  }, &route);
  // From a child process: this realm, the parent, and the other children.
  if (route.children)
    route.children->SetBroadcastSink([](void* context, std::string_view frame, const void* from) {
      auto& r = *static_cast<BroadcastRoute*>(context);
      if (!Deliver(r, frame)) return false;
      if (r.parent && !r.parent->Queue(std::string(frame))) return false;
      return r.children->Broadcast(std::string(frame), from);
    }, &route);
  // From the parent process: this realm and every child below it.
  if (route.parent)
    route.parent->SetBroadcastPort([](void* context, std::string_view frame) {
      auto& r = *static_cast<BroadcastRoute*>(context);
      return Deliver(r, frame) && (!r.children || r.children->Broadcast(std::string(frame), nullptr));
    }, &route);
}
void UnbindBroadcastRoute(BroadcastRoute& route) {
  route.channels.ClearRelayPort(&route);
  if (route.children) route.children->SetBroadcastSink(nullptr, nullptr);
  if (route.parent) route.parent->SetBroadcastPort(nullptr, nullptr);
}
}  // namespace ruyijs
