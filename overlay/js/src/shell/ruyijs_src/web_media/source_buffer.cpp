#include "web_media/media_codecs.h"
#include "web_media/media_runtime.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/ValueArray.h"
namespace ruyijs::web_media {
const JSClass kSourceBufferClass = {"SourceBuffer", 0};
const JSClass kSourceBufferListClass = {"SourceBufferList", JSCLASS_HAS_RESERVED_SLOTS(ListSlotCount)};
bool SourceBufferMember(JSContext* cx, unsigned, JS::Value*) {
  return WebKit::Fail(cx, "Illegal SourceBuffer receiver");
}
bool SourceBufferListMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  return MediaReceiver(cx, args.thisv(), &kSourceBufferListClass, &self) &&
      MediaGetSlot(cx, self, WebKit::Slot(args), args.rval());
}
bool SourceBufferListSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!MediaReceiver(cx, args.thisv(), &kSourceBufferListClass, &self)) return false;
  if (!argc) return WebKit::Fail(cx, "SourceBufferList setter requires an argument");
  auto& state = *static_cast<State*>(JS::GetReservedSlot(self, ListOwner).toPrivate());
  const int slot = WebKit::Slot(args);
  if (!MediaHandlerSet(state, self, slot, slot == ListAdd ? "addsourcebuffer" : "removesourcebuffer", args[0])) return false;
  args.rval().setUndefined(); return true;
}
bool SourceBufferListIterator(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!MediaReceiver(cx, args.thisv(), &kSourceBufferListClass, &self)) return false;
  auto& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject array(cx, JS::NewArrayObject(cx, 0));
  return array && JS::Call(cx, array, state.arrayValues, JS::HandleValueArray::empty(), args.rval());
}
}
