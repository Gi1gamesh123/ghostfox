#include "web_rtc/internal.h"

#include "js/Realm.h"

namespace ruyijs::web_rtc {

JSObject* NewDescription(State& state, const DescriptionSpec& spec, JS::HandleObject proto) {
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.kit.page);
  JS::RootedObject description(cx, state.kit.New(&kDescriptionClass, proto));
  JS::RootedValue type(cx), sdp(cx);
  if (!description || !StringValue(cx, spec.type, &type) ||
      !StringValue(cx, spec.sdp, &sdp))
    return nullptr;
  JS::SetReservedSlot(description, kDescriptionType, type);
  JS::SetReservedSlot(description, kDescriptionSdp, sdp);
  return description.get();
}

// RTCIceCandidate mirrors the ruyijs shape: candidate defaults to "", the other
// three members default to null.
JSObject* NewCandidate(State& state, JS::HandleValue dictionary, JS::HandleObject proto) {
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.kit.page);
  std::string text, mid, user;
  bool hasMid = false, hasIndex = false, hasUser = false;
  int32_t index = 0;
  if (dictionary.isObject()) {
    JS::RootedObject init(cx, &dictionary.toObject());
    if (!ReadString(cx, init, "candidate", &text) ||
        !ReadNullable(cx, init, "sdpMid", &hasMid, &mid) ||
        !ReadNullable(cx, init, "usernameFragment", &hasUser, &user) ||
        !ReadInt(cx, init, "sdpMLineIndex", &hasIndex, &index))
      return nullptr;
  }
  JS::RootedObject candidate(cx, state.kit.New(&kCandidateClass, proto));
  JS::RootedValue candidateText(cx), candidateMid(cx, JS::NullValue());
  JS::RootedValue candidateIndex(cx, JS::NullValue());
  JS::RootedValue candidateUser(cx, JS::NullValue());
  if (!candidate || !StringValue(cx, text, &candidateText)) return nullptr;
  if (hasMid && !StringValue(cx, mid, &candidateMid)) return nullptr;
  if (hasUser && !StringValue(cx, user, &candidateUser)) return nullptr;
  if (hasIndex) candidateIndex.setInt32(index);
  JS::SetReservedSlot(candidate, kCandidateText, candidateText);
  JS::SetReservedSlot(candidate, kCandidateMid, candidateMid);
  JS::SetReservedSlot(candidate, kCandidateIndex, candidateIndex);
  JS::SetReservedSlot(candidate, kCandidateUser, candidateUser);
  return candidate.get();
}

bool DescriptionCreate(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject proto(cx);
  if (!state->kit.ConstructProto(args, state->description.prototype, &proto)) return false;
  if (!args.get(0).isObject())
    return WebKit::Fail(cx, "RTCSessionDescription requires an init dictionary");
  JS::RootedObject init(cx, &args[0].toObject());
  DescriptionSpec spec;
  if (!ReadString(cx, init, "type", &spec.type) || !ReadString(cx, init, "sdp", &spec.sdp))
    return false;
  JS::RootedObject description(cx, NewDescription(*state, spec, proto));
  if (!description) return false;
  args.rval().setObject(*description);
  return true;
}

bool CandidateCreate(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject proto(cx);
  if (!state->kit.ConstructProto(args, state->candidate.prototype, &proto)) return false;
  JS::RootedObject candidate(cx, NewCandidate(*state, args.get(0), proto));
  if (!candidate) return false;
  args.rval().setObject(*candidate);
  return true;
}

bool DescriptionGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Brand(cx, args, &kDescriptionClass, &self))
    return false;
  args.rval().set(JS::GetReservedSlot(self, WebKit::Slot(args)));
  return true;
}

bool CandidateGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Brand(cx, args, &kCandidateClass, &self))
    return false;
  args.rval().set(JS::GetReservedSlot(self, WebKit::Slot(args)));
  return true;
}

bool DescriptionMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Brand(cx, args, &kDescriptionClass, &self))
    return false;
  JS::RootedObject json(cx, JS_NewPlainObject(cx));
  JS::RootedValue type(cx, JS::GetReservedSlot(self, kDescriptionType));
  JS::RootedValue sdp(cx, JS::GetReservedSlot(self, kDescriptionSdp));
  if (!json || !JS_DefineProperty(cx, json, "type", type, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, json, "sdp", sdp, JSPROP_ENUMERATE))
    return false;
  args.rval().setObject(*json);
  return true;
}

bool CandidateMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Brand(cx, args, &kCandidateClass, &self))
    return false;
  JS::RootedObject json(cx, JS_NewPlainObject(cx));
  if (!json) return false;
  // Keep Firefox's WebIDL dictionary order: candidate, sdpMLineIndex, sdpMid,
  // usernameFragment. Values come from the candidate's realm-local slots.
  JS::RootedValue candidate(cx, JS::GetReservedSlot(self, kCandidateText));
  JS::RootedValue index(cx, JS::GetReservedSlot(self, kCandidateIndex));
  JS::RootedValue mid(cx, JS::GetReservedSlot(self, kCandidateMid));
  JS::RootedValue user(cx, JS::GetReservedSlot(self, kCandidateUser));
  if (!JS_DefineProperty(cx, json, "candidate", candidate, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, json, "sdpMLineIndex", index, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, json, "sdpMid", mid, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, json, "usernameFragment", user, JSPROP_ENUMERATE))
    return false;
  args.rval().setObject(*json);
  return true;
}

}  // namespace ruyijs::web_rtc
