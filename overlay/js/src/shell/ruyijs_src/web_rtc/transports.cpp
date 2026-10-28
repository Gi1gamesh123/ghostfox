// RTCDtlsTransport, RTCIceTransport, RTCSctpTransport and RTCRtpScriptTransform
// (RTCStatsReport: stats_report.cpp) (docs/owners/WEB_RTC.md, 2026-10-21). Each object's
// observable state is its reserved slots, like every other interface of this
// owner. Without an ICE/DTLS agent or getStats no path creates the three
// transports or a stats report yet; their members still brand-check and read
// the slot layout an instance factory will fill.
#include "web_rtc/state.h"
#include "runtime/page_helpers.h"

#include <iterator>
#include <string>

#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/ForOfIterator.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/ValueArray.h"

namespace ruyijs::web_rtc {
const JSClass kDtlsClass = {"RTCDtlsTransport", JSCLASS_HAS_RESERVED_SLOTS(kDtlsSlotCount)};
const JSClass kIceClass = {"RTCIceTransport", JSCLASS_HAS_RESERVED_SLOTS(kIceSlotCount)};
const JSClass kSctpClass = {"RTCSctpTransport", JSCLASS_HAS_RESERVED_SLOTS(kSctpSlotCount)};
const JSClass kScriptTransformClass = {"RTCRtpScriptTransform",
                                       JSCLASS_HAS_RESERVED_SLOTS(kScriptSlotCount)};
namespace {
struct Member { const char* name; int32_t slot; };
// Firefox's prototype order: the operation first, then attributes, then handlers.
const Member kDtlsProps[] = {{"iceTransport", kDtlsIceTransport}, {"state", kDtlsState}};
const Member kIceProps[] = {
    {"role", kIceRole}, {"state", kIceState}, {"gatheringState", kIceGatheringState}};
const Member kSctpProps[] = {{"transport", kSctpTransport}, {"state", kSctpState},
                             {"maxMessageSize", kSctpMaxMessageSize},
                             {"maxChannels", kSctpMaxChannels}};

bool NotImplemented(JSContext* cx, const char* member, const char* iface) {
  const std::string text = std::string("'") + member +
      "' called on an object that does not implement interface " + iface + ".";
  return WebKit::Fail(cx, text.c_str());
}
const JSClass* ClassFor(const char* iface) {
  const std::string name(iface);
  return name == "RTCDtlsTransport" ? &kDtlsClass : name == "RTCIceTransport" ? &kIceClass
                                     : &kSctpClass;
}
// The callee's interface name rides in the getter's own name ("get <attr>").
bool Receiver(const JS::CallArgs& args, const char* iface, const char* member,
              JS::MutableHandleObject self) {
  JSContext* cx = static_cast<State*>(WebKit::Owner(args))->cx;
  JSObject* object = args.thisv().isObject() ? &args.thisv().toObject() : nullptr;
  if (!object || JS::GetClass(object) != ClassFor(iface)) return NotImplemented(cx, member, iface);
  self.set(object);
  return true;
}
template <size_t N>
const char* NameOf(const Member (&props)[N], int32_t slot) {
  for (const Member& m : props) if (m.slot == slot) return m.name;
  return "";
}
template <const char* const* Iface>
bool SlotGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = WebKit::Slot(args);
  const JSClass* clasp = ClassFor(*Iface);
  const char* name = clasp == &kDtlsClass ? NameOf(kDtlsProps, slot)
                   : clasp == &kIceClass ? NameOf(kIceProps, slot) : NameOf(kSctpProps, slot);
  JS::RootedObject self(cx);
  if (!Receiver(args, *Iface, (std::string("get ") + name).c_str(), &self)) return false;
  args.rval().set(JS::GetReservedSlot(self, slot));
  return true;
}
const char* const kDtlsName = "RTCDtlsTransport";
const char* const kIceName = "RTCIceTransport";
const char* const kSctpName = "RTCSctpTransport";

bool TransportMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (WebKit::Slot(args) == kDtlsGetRemoteCertificates) {
    if (!Receiver(args, kDtlsName, "getRemoteCertificates", &self)) return false;
    JSObject* none = JS::NewArrayObject(cx, 0);  // No DTLS handshake: no remote certificate.
    if (!none) return false;
    args.rval().setObject(*none);
    return true;
  }
  if (!Receiver(args, kIceName, "getSelectedCandidatePair", &self)) return false;
  args.rval().setNull();  // No ICE agent: no candidate pair is ever selected.
  return true;
}

// new RTCRtpScriptTransform(worker, options, transfer): the WebIDL conversions
// of Firefox's constructor. No encoded-frame pipeline exists, so no
// rtctransform event reaches the worker (recorded deviation).
bool ScriptTransformCreate(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  if (!args.isConstructing())
    return WebKit::Fail(cx, "RTCRtpScriptTransform constructor: 'new' is required");
  if (!args.length())
    return WebKit::Fail(cx, "RTCRtpScriptTransform constructor: At least 1 argument required, but only 0 passed");
  if (!args[0].isObject() || !s.workerCheck || !s.workerCheck(&args[0].toObject()))
    return WebKit::Fail(cx, "RTCRtpScriptTransform constructor: Argument 1 does not implement interface Worker.");
  if (!args.get(2).isUndefined()) {
    JS::ForOfIterator it(cx);
    if (!args[2].isObject() || !it.init(args[2], JS::ForOfIterator::AllowNonIterable) ||
        !it.valueIsIterable())
      return WebKit::Fail(cx, "RTCRtpScriptTransform constructor: Argument 3 can't be converted to a sequence.");
    JS::RootedValue item(cx);
    for (bool done = false;;) {
      if (!it.next(&item, &done)) return false;
      if (done) break;
      if (!item.isObject())
        return WebKit::Fail(cx, "RTCRtpScriptTransform constructor: Element of argument 3 is not an object.");
    }
  }
  JS::RootedObject proto(cx);
  if (!s.kit.ConstructProto(args, s.scriptTransform.prototype, &proto)) return false;
  JS::RootedObject object(cx, s.kit.New(&kScriptTransformClass, proto));
  if (!object) return false;
  JS::SetReservedSlot(object, kScriptWorker, args[0]);
  JS::SetReservedSlot(object, kScriptOptions, args.get(1));
  args.rval().setObject(*object);
  return true;
}

template <size_t N>
bool Props(State& s, WebFace& face, const char* iface, const Member (&props)[N], JSNative getter) {
  for (const Member& m : props)
    if (!s.kit.Accessor(face, iface, m.name, getter, nullptr, m.slot)) return false;
  return true;
}
bool Handlers(State& s, WebFace& face, const char* iface, const char* const* names,
              int32_t count, int32_t base) {
  for (int32_t i = 0; i < count; ++i) {
    const std::string name = std::string("on") + names[i];
    if (!s.kit.Accessor(face, iface, name.c_str(), HandlerGet, HandlerSet,
                        HandlerSlot(ClassFor(iface), base + i))) return false;
  }
  return true;
}
}  // namespace

bool InstallTransports(State& s) {
  JSContext* cx = s.cx;
  JS::RootedObject objectProto(cx, JS::GetRealmObjectPrototype(cx));
  JS::RootedObject targetProto(cx, s.events->prototype());
  if (!objectProto || !targetProto ||
      !s.kit.Define(kDtlsName, &kDtlsClass, targetProto, IllegalCreate, &s.dtls) ||
      !InstallError(s) ||
      !s.kit.Define(kIceName, &kIceClass, targetProto, IllegalCreate, &s.ice) ||
      !s.kit.Define(kSctpName, &kSctpClass, targetProto, IllegalCreate, &s.sctp) ||
      !InstallStatsReport(s, objectProto) ||
      !s.kit.Define("RTCRtpScriptTransform", &kScriptTransformClass, objectProto,
                    ScriptTransformCreate, &s.scriptTransform, 1) ||
      !InstallEncodedFrames(s, objectProto) || !InstallNegotiationViews(s))
    return false;
  if (!s.kit.Method(s.dtls, kDtlsName, "getRemoteCertificates", TransportMethod, 0,
                    kDtlsGetRemoteCertificates) ||
      !Props(s, s.dtls, kDtlsName, kDtlsProps, SlotGet<&kDtlsName>) ||
      !Handlers(s, s.dtls, kDtlsName, kDtlsHandlers, kDtlsHandlerCount, kDtlsHandlerBase) ||
      !s.kit.Method(s.ice, kIceName, "getSelectedCandidatePair", TransportMethod, 0,
                    kIceGetSelectedCandidatePair) ||
      !Props(s, s.ice, kIceName, kIceProps, SlotGet<&kIceName>) ||
      !Handlers(s, s.ice, kIceName, kIceHandlers, kIceHandlerCount, kIceHandlerBase) ||
      !Props(s, s.sctp, kSctpName, kSctpProps, SlotGet<&kSctpName>) ||
      !Handlers(s, s.sctp, kSctpName, kSctpHandlers, kSctpHandlerCount, kSctpHandlerBase))
    return false;
  return true;
}
}  // namespace ruyijs::web_rtc
