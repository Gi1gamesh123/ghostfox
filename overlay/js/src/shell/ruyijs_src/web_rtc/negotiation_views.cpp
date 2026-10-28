// RTCDTMFSender and RTCIceCandidatePair (Firefox 158 RTCDTMFSender.webidl,
// RTCIceCandidatePair.webidl, Exposed=Window). Firefox makes them only for a
// negotiated audio sender (RTCRtpSender.dtmf) and a selected ICE pair; this owner
// negotiates no media and selects no pair (D136), so neither has instances: the
// interface objects keep Firefox's shape and every member fails its brand check.
#include "web_rtc/state.h"

#include <string>

namespace ruyijs::web_rtc {
namespace {
const JSClass kDtmfClass = {"RTCDTMFSender", 0};
const JSClass kPairClass = {"RTCIceCandidatePair", 0};
// Slot: (interface << 8) | member; interface 0 is RTCDTMFSender, 1 RTCIceCandidatePair.
const char* const kDtmf[] = {"insertDTMF", "ontonechange", "canInsertDTMF", "toneBuffer"};
const char* const kPair[] = {"local", "remote"};
bool Brand(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = WebKit::Slot(args);
  const bool dtmf = (slot >> 8) == 0;
  std::string label;
  return WebKit::BrandText(cx, args, dtmf ? "RTCDTMFSender" : "RTCIceCandidatePair", &label) &&
         WebKit::Fail(cx, label.c_str());
}
}  // namespace

bool InstallNegotiationViews(State& s) {
  JS::RootedObject objectProto(s.cx, JS::GetRealmObjectPrototype(s.cx));
  JS::RootedObject targetProto(s.cx, s.events->prototype());
  WebFace dtmf(s.cx), pair(s.cx);
  if (!s.kit.Define("RTCDTMFSender", &kDtmfClass, targetProto, IllegalCreate, &dtmf) ||
      !s.kit.Method(dtmf, "RTCDTMFSender", kDtmf[0], Brand, 1, 0) ||
      !s.kit.Accessor(dtmf, "RTCDTMFSender", kDtmf[1], Brand, Brand, 1) ||
      !s.kit.Accessor(dtmf, "RTCDTMFSender", kDtmf[2], Brand, nullptr, 2) ||
      !s.kit.Accessor(dtmf, "RTCDTMFSender", kDtmf[3], Brand, nullptr, 3) ||
      !s.kit.Define("RTCIceCandidatePair", &kPairClass, objectProto, IllegalCreate, &pair))
    return false;
  for (int32_t i = 0; i < 2; ++i)
    if (!s.kit.Accessor(pair, "RTCIceCandidatePair", kPair[i], Brand, nullptr, (1 << 8) | i)) return false;
  return true;
}
}  // namespace ruyijs::web_rtc
