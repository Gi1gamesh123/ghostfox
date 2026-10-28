// ICE gathering for a new local description (docs/owners/WEB_RTC.md,
// 2026-10-21). Firefox trickles host candidates per m-line after
// setLocalDescription resolves: component 1 for every m-line, component 2 for
// media m-lines whose RTCP is not yet muxed, per local interface a UDP
// candidate on a fresh port and a TCP active candidate on port 9, each with an
// mDNS name (one UUID per interface per connection); then a "" marker per
// section, gathering completes, and the final null event. Without UDP reachability
// to STUN no srflx candidate appears. Configured "rtc" iceCandidates replace
// the generated host set.
#include "web_rtc/internal.h"
#include "web_rtc/profile.h"
#include "web_rtc/ice_sections.h"
#include "interaction/realm.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"
#include <cstdio>
#include <random>

namespace ruyijs::web_rtc {
namespace {
struct Line { std::string text, mid; int32_t index; };
std::mt19937_64& Random() { static std::mt19937_64 random{std::random_device{}()}; return random; }
std::string Uuid() {
  const uint64_t a = Random()(), b = Random()();
  char text[40];
  std::snprintf(text, sizeof text, "%08x-%04x-4%03x-%04x-%012llx", unsigned(a >> 32), unsigned(a >> 16) & 0xffff,
                unsigned(a) & 0xfff, unsigned(0x8000 | (b >> 48 & 0x3fff)), (unsigned long long)(b & 0xffffffffffffULL));
  return text;
}
std::vector<Line> HostCandidates(const std::vector<IceSection>& sections) {
  // Firefox priorities of its two local interfaces (UDP host, TCP active host).
  static const uint32_t kUdp[] = {2122187007u, 2122252543u}, kTcp[] = {2105458943u, 2105524479u};
  const std::string names[] = {Uuid() + ".local", Uuid() + ".local"};
  uint32_t port = 49152 + uint32_t(Random()() % 14000);
  std::vector<Line> lines;
  for (size_t m = 0; m < sections.size(); ++m) {
    const int components = sections[m].kind == "application" ? 1 : 2;
    for (int component = 1; component <= components; ++component) {
      for (int i = 0; i < 2; ++i)
        lines.push_back({"candidate:" + std::to_string(i * 2) + " " + std::to_string(component) + " UDP " +
                             std::to_string(kUdp[i] - uint32_t(component - 1)) + " " + names[i] + " " +
                             std::to_string(port++) + " typ host", sections[m].mid, sections[m].index});
      for (int i = 0; i < 2; ++i)
        lines.push_back({"candidate:" + std::to_string(4 + i) + " " + std::to_string(component) + " TCP " +
                             std::to_string(kTcp[i] - uint32_t(component - 1)) + " " + names[i] +
                             " 9 typ host tcptype active", sections[m].mid, sections[m].index});
    }
  }
  return lines;
}
bool Candidate(State& state, const Line& line, const std::string& ufrag, JS::MutableHandleValue out) {
  JSContext* cx = state.cx;
  JS::RootedObject init(cx, JS_NewPlainObject(cx));
  JS::RootedValue text(cx), mid(cx), index(cx, JS::Int32Value(line.index)), user(cx, JS::NullValue());
  if (!init || !StringValue(cx, line.text, &text) || !StringValue(cx, line.mid, &mid)) return false;
  if (!ufrag.empty() && !StringValue(cx, ufrag, &user)) return false;
  if (!JS_DefineProperty(cx, init, "candidate", text, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "sdpMid", mid, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "sdpMLineIndex", index, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "usernameFragment", user, JSPROP_ENUMERATE)) return false;
  JS::RootedValue dictionary(cx, JS::ObjectValue(*init));
  JS::RootedObject candidate(cx, NewCandidate(state, dictionary, state.candidate.prototype));
  if (!candidate) return false;
  out.setObject(*candidate);
  return true;
}
// One gathering step: pop and fire the next candidate (null once the list is
// empty), then re-post itself; the final step completes gathering. The list is
// stored in reverse firing order so the task needs no cursor slot: a native
// function has only FunctionExtended::NUM_EXTENDED_SLOTS (3) reserved slots,
// 0 state, 1 peer, 2 list.
bool Step(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject callee(cx, &args.callee());
  State& state = *static_cast<State*>(js::GetFunctionNativeReserved(callee, 0).toPrivate());
  JS::RootedObject peer(cx, &js::GetFunctionNativeReserved(callee, 1).toObject());
  JS::RootedObject list(cx, &js::GetFunctionNativeReserved(callee, 2).toObject());
  args.rval().setUndefined();
  PeerInfo info;
  if (!state.model.peer_info(PeerId(peer), &info) || info.closed) return true;
  uint32_t length = 0;
  JS::RootedValue candidate(cx);
  if (!JS::GetArrayLength(cx, list, &length)) return false;
  if (length == 0) {
    if (!state.model.finish_gathering(PeerId(peer))) return true;
    if (!SyncPeer(state, peer)) return false;
    if (!state.model.peer_info(PeerId(peer), &info) || info.closed) return true;
    candidate.setNull();
    return state.events->FireIceCandidate(peer, candidate);
  }
  if (!JS_GetElement(cx, list, length - 1, &candidate) || !JS::SetArrayLength(cx, list, length - 1) ||
      !state.events->FireIceCandidate(peer, candidate)) return false;
  return state.postTask(state.postTaskContext, callee, 1 + double(Random()() % 4));
}
}  // namespace

bool StartGathering(State& state, JS::HandleObject peer, const std::string& sdp) {
  JSContext* cx = state.cx;
  if (!state.postTask) return true;
  PeerInfo info;
  if (!state.model.peer_info(PeerId(peer), &info) || info.closed) return true;
  std::string ufrag;
  auto sections = IceSections(sdp, info.data_transport_position, &ufrag);
  std::string firstMid = "0";
  for (const auto& section : sections) if (section.index == 0) firstMid = section.mid;
  std::vector<Line> lines;
  for (const auto& text : CurrentProfile().iceCandidates) lines.push_back({text, firstMid, 0});
  if (lines.empty()) lines = HostCandidates(sections);
  for (const auto& section : sections) lines.push_back({"", section.mid, section.index});
  JS::RootedObject list(cx, JS::NewArrayObject(cx, 0));
  if (!list) return false;
  const uint32_t count = uint32_t(lines.size());
  for (uint32_t i = 0; i < count; ++i) {  // reversed: Step pops from the end
    JS::RootedValue candidate(cx);
    if (!Candidate(state, lines[i], ufrag, &candidate) || !JS_SetElement(cx, list, count - 1 - i, candidate)) return false;
  }
  JSFunction* step = js::NewFunctionWithReserved(cx, Step, 0, 0, "ICE gathering");
  if (!step) return false;
  JS::RootedObject task(cx, JS_GetFunctionObject(step));
  js::SetFunctionNativeReserved(task, 0, JS::PrivateValue(&state));
  js::SetFunctionNativeReserved(task, 1, JS::ObjectValue(*peer));
  js::SetFunctionNativeReserved(task, 2, JS::ObjectValue(*list));
  return state.postTask(state.postTaskContext, task, 2 + double(Random()() % 6));
}
}  // namespace ruyijs::web_rtc
