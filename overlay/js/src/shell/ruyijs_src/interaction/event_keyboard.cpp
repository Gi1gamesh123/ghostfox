#include "interaction/event_keyboard.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/String.h"

namespace ruyijs::interaction {
domain::KeyboardMessage KeyboardMessageOf(Event* event) {
  // Text() stores the existing dispatch key as UTF-16 bytes. Read that same
  // immutable key instead of consulting overridable public type getters.
  const std::string_view key = event->value.type().value();
  if (key == std::string_view(reinterpret_cast<const char*>(u"keydown"), 14)) return domain::KeyboardMessage::Down;
  if (key == std::string_view(reinterpret_cast<const char*>(u"keyup"), 10)) return domain::KeyboardMessage::Up;
  if (key == std::string_view(reinterpret_cast<const char*>(u"keypress"), 16)) return domain::KeyboardMessage::Press;
  return domain::KeyboardMessage::Other;
}
uint32_t KeyboardCodeValue(JSContext* cx, Event* event, domain::KeyboardCode field) {
  const uint32_t character = uint32_t(JS::GetReservedSlot(event->object, kKeyboardCharCode).toNumber());
  const uint32_t key = uint32_t(JS::GetReservedSlot(event->object, kKeyboardKeyCode).toNumber());
  if (JS::GetReservedSlot(event->object, kEventKeyboardFlags).toInt32() & kKeyboardConstructed) {
    if (field == domain::KeyboardCode::Character) return character;
    if (field == domain::KeyboardCode::Key) return key;
    return uint32_t(JS::GetReservedSlot(event->object, kKeyboardWhich).toNumber());
  }
  return domain::KeyboardLegacyCode(KeyboardMessageOf(event), field, character, key);
}
namespace {
bool Member(JSContext* cx, unsigned argc, JS::Value* vp, const char* name, unsigned slot,
            uint32_t mask = 0) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::Keyboard, true);
  if (!event) return false;
  ReportEventAccess(cx, event, "KeyboardEvent", name, int(ApiAccess::Read));
  args.rval().set(JS::GetReservedSlot(event->object, slot));
  if (mask) args.rval().setBoolean((uint32_t(args.rval().toInt32()) & mask) != 0);
  return JS_WrapValue(cx, args.rval());
}
#define KEYBOARD_MEMBER(fn, name, slot, mask) \
  bool fn(JSContext* cx, unsigned argc, JS::Value* vp) { return Member(cx, argc, vp, name, slot, mask); }
KEYBOARD_MEMBER(Key, "key", kEventKey, 0)
KEYBOARD_MEMBER(Code, "code", kEventCode, 0)
KEYBOARD_MEMBER(Location, "location", kEventLocation, 0)
KEYBOARD_MEMBER(Repeat, "repeat", kEventKeyboardFlags, 1)
KEYBOARD_MEMBER(Composing, "isComposing", kEventKeyboardFlags, 2)
KEYBOARD_MEMBER(Alt, "altKey", kEventModifiers, domain::kAltMask)
KEYBOARD_MEMBER(Control, "ctrlKey", kEventModifiers, domain::kControlMask)
KEYBOARD_MEMBER(Shift, "shiftKey", kEventModifiers, domain::kShiftMask)
KEYBOARD_MEMBER(Meta, "metaKey", kEventModifiers, domain::kMetaMask)
#undef KEYBOARD_MEMBER
bool CodeMember(JSContext* cx, unsigned argc, JS::Value* vp, domain::KeyboardCode field) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::Keyboard, true);
  if (!event) return false;
  ReportEventAccess(cx, event, "KeyboardEvent", field == domain::KeyboardCode::Key ? "keyCode" : "charCode",
                    int(ApiAccess::Read));
  args.rval().setNumber(KeyboardCodeValue(cx, event, field));
  return true;
}
bool CharCode(JSContext* cx, unsigned argc, JS::Value* vp) {
  return CodeMember(cx, argc, vp, domain::KeyboardCode::Character);
}
bool KeyCode(JSContext* cx, unsigned argc, JS::Value* vp) {
  return CodeMember(cx, argc, vp, domain::KeyboardCode::Key);
}
}
bool GetUiWhich(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::UI, true);
  if (!event) return false;
  ReportEventAccess(cx, event, "UIEvent", "which", int(ApiAccess::Read));
  args.rval().setNumber(event->kind == Kind::Keyboard ?
      KeyboardCodeValue(cx, event, domain::KeyboardCode::Which) : 0);
  return true;
}
bool GetKeyboardModifierState(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::Keyboard, true);
  if (!event) return false;
  ReportEventAccess(cx, event, "KeyboardEvent", "getModifierState", int(ApiAccess::Call));
  if (!argc) return Error(cx, "TypeError", "getModifierState requires a key");
  JS::RootedString key(cx, JS::ToString(cx, args[0]));
  if (!key) return false;
  const uint32_t mask = uint32_t(JS::GetReservedSlot(event->object, kEventModifiers).toInt32());
  args.rval().setBoolean(false);
  for (const auto& query : domain::kModifierQueries) {
    bool equal = false;
    if (!JS_StringEqualsAscii(cx, key, query.member, &equal)) return false;
    if (equal) { args.rval().setBoolean((mask & query.mask) != 0); break; }
  }
  return true;
}
extern const JSPropertySpec kKeyboardProperties[] = {
  JS_PSG("charCode", CharCode, JSPROP_ENUMERATE), JS_PSG("keyCode", KeyCode, JSPROP_ENUMERATE),
  JS_PSG("altKey", Alt, JSPROP_ENUMERATE), JS_PSG("ctrlKey", Control, JSPROP_ENUMERATE),
  JS_PSG("shiftKey", Shift, JSPROP_ENUMERATE), JS_PSG("metaKey", Meta, JSPROP_ENUMERATE),
  JS_PSG("location", Location, JSPROP_ENUMERATE), JS_PSG("repeat", Repeat, JSPROP_ENUMERATE),
  JS_PSG("isComposing", Composing, JSPROP_ENUMERATE), JS_PSG("key", Key, JSPROP_ENUMERATE),
  JS_PSG("code", Code, JSPROP_ENUMERATE), JS_PS_END};
extern const JSFunctionSpec kKeyboardMethods[] = {
  JS_FN("getModifierState", GetKeyboardModifierState, 1, JSPROP_ENUMERATE),
  JS_FN("initKeyboardEvent", InitKeyboardEvent, 1, JSPROP_ENUMERATE), JS_FS_END};
}
