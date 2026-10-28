#include "interaction/data_transfer.h"
#include "js/Array.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "js/Realm.h"

namespace ruyijs::interaction {
namespace {
enum Property { Drop, Effect, Items, Types, Files };
State* TransferState(JSContext* cx) {
  State* state = activeState;
  while (state && state->page != JS::CurrentGlobalOrNull(cx)) state = state->previous;
  return state;
}
bool PropertyCall(JSContext* cx, const JS::CallArgs& args, Property property, bool setter) {
  JS::RootedObject object(cx);
  TransferModel* model = TransferReceiver(cx, args, &object);
  if (!model) return false;
  static const char* names[] = {"dropEffect", "effectAllowed", "items", "types", "files"};
  ObserveTransfer(cx, "DataTransfer", names[property], setter ? ApiAccess::Write : ApiAccess::Read);
  if (setter) {
    std::string text;
    if (!TransferString(cx, args.get(0), &text)) return false;
    SetTransferEffect(*model, property == Drop, text);
    args.rval().setUndefined(); return true;
  }
  if (property == Items) {
    args.rval().set(JS::GetReservedSlot(object, kTransferItems));
    return JS_WrapValue(cx, args.rval());
  }
  if (property == Files) {
    // This DataTransfer holds string items only, so its FileList is empty.
    State* state = TransferState(cx);
    if (!state || !state->fileList) return Error(cx, "TypeError", "FileList factory is not installed");
    JS::RootedObject list(cx), none(cx);
    if (!state->fileList(state->fileListContext, none, &list)) return false;
    args.rval().setObject(*list);
    return JS_WrapValue(cx, args.rval());
  }
  if (property == Drop || property == Effect) {
    JS::RootedString text(cx, JS_NewStringCopyZ(cx, TransferEffect(*model, property == Drop)));
    if (!text) return false;
    args.rval().setString(text); return true;
  }
  std::vector<std::string> formats;
  if (!TransferResult(cx, TransferTypes(*model, &formats))) return false;
  JS::RootedObject result(cx, JS::NewArrayObject(cx, formats.size()));
  if (!result) return false;
  for (size_t i = 0; i < formats.size(); ++i) {
    JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx,
        JS::UTF8Chars(formats[i].data(), formats[i].size())));
    JS::RootedValue value(cx);
    if (!text) return false;
    value.setString(text);
    if (!JS_DefineElement(cx, result, i, value, JSPROP_ENUMERATE)) return false;
  }
  if (!JS_FreezeObject(cx, result)) return false;
  args.rval().setObject(*result); return true;
}
#define PROPERTY(fn, property, setter) \
  bool fn(JSContext* cx, unsigned argc, JS::Value* vp) { \
    return PropertyCall(cx, JS::CallArgsFromVp(argc, vp), property, setter); \
  }
PROPERTY(GetDrop, Drop, false)
PROPERTY(SetDrop, Drop, true)
PROPERTY(GetEffect, Effect, false)
PROPERTY(SetEffect, Effect, true)
PROPERTY(GetItems, Items, false)
PROPERTY(GetTypes, Types, false)
PROPERTY(GetFiles, Files, false)
#undef PROPERTY
}
extern const JSPropertySpec kTransferProperties[] = {
  JS_PSGS("dropEffect", GetDrop, SetDrop, JSPROP_ENUMERATE),
  JS_PSGS("effectAllowed", GetEffect, SetEffect, JSPROP_ENUMERATE),
  JS_PSG("items", GetItems, JSPROP_ENUMERATE),
  JS_PSG("types", GetTypes, JSPROP_ENUMERATE),
  JS_PSG("files", GetFiles, JSPROP_ENUMERATE), JS_PS_END};
}  // namespace ruyijs::interaction
