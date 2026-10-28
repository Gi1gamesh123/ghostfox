#include "interaction/data_transfer.h"
#include "runtime/runtime.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/StableStringChars.h"
#include "js/Wrapper.h"

namespace ruyijs::interaction {
void ObserveTransfer(JSContext* cx, const char* iface, const char* name, ApiAccess access) {
  if (State* state = RealmState(cx)) state->Observe(iface, name, int(access));
}
TransferModel* TransferReceiver(JSContext* cx, const JS::CallArgs& args,
                                JS::MutableHandleObject object) {
  object.set(args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!object || JS::GetClass(object) != &kTransferClass ||
      JS::GetReservedSlot(object, kTransferModel).isUndefined()) {
    Error(cx, "TypeError", "DataTransfer receiver is incompatible"); return nullptr;
  }
  return static_cast<TransferModel*>(JS::GetReservedSlot(object, kTransferModel).toPrivate());
}
bool TransferListReceiver(JSContext* cx, const JS::CallArgs& args,
                           JS::MutableHandleObject object) {
  object.set(args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!object || JS::GetClass(object) != &kTransferListClass ||
      !JS::GetReservedSlot(object, 0).isObject())
    return Error(cx, "TypeError", "DataTransferItemList receiver is incompatible");
  return true;
}
bool TransferResult(JSContext* cx, TransferStatus result) {
  if (result == TransferStatus::NoMemory) { JS_ReportOutOfMemory(cx); return false; }
  return result == TransferStatus::Ok || Error(cx, "RangeError", "DataTransfer payload limit exceeded");
}
bool TransferString(JSContext* cx, JS::HandleValue input, std::string* text) {
  JS::RootedString string(cx, JS::ToString(cx, input));
  if (!string) return false;
  if (JS_GetStringLength(string) > 1024 * 1024)
    return Error(cx, "RangeError", "DataTransfer string limit exceeded");
  if (!ToUTF8(cx, string, text)) return false;
  return text->size() <= 1024 * 1024 || Error(cx, "RangeError", "DataTransfer string limit exceeded");
}
bool TransferText(JSContext* cx, JS::HandleValue input, std::u16string* text) {
  JS::RootedString string(cx, JS::ToString(cx, input));
  if (!string) return false;
  if (JS_GetStringLength(string) > 512 * 1024)
    return Error(cx, "RangeError", "DataTransfer text limit exceeded");
  JS::AutoStableStringChars chars(cx);
  if (!chars.initTwoByte(cx, string)) return false;
  text->assign(chars.twoByteChars(), chars.length());
  return true;
}
namespace {
enum Method { Clear, Get, Set, DragImage };
bool Call(JSContext* cx, const JS::CallArgs& args, Method method) {
  JS::RootedObject object(cx);
  TransferModel* model = TransferReceiver(cx, args, &object);
  if (!model) return false;
  static const char* names[] = {"clearData", "getData", "setData", "setDragImage"};
  ObserveTransfer(cx, "DataTransfer", names[method], ApiAccess::Call);
  const unsigned required = method == Get ? 1 : method == Set ? 2 : method == DragImage ? 3 : 0;
  if (args.length() < required) return Error(cx, "TypeError", "DataTransfer method needs more arguments");
  args.rval().setUndefined();
  if (method == DragImage) {
    int32_t x, y;
    return JS::ToInt32(cx, args[1], &x) && JS::ToInt32(cx, args[2], &y);
  }
  std::string format;
  if ((method != Clear || args.hasDefined(0)) && !TransferString(cx, args.get(0), &format)) return false;
  if (method == Clear) return TransferResult(cx, TransferClear(*model, format));
  std::u16string text;
  if (method == Set)
    return TransferText(cx, args[1], &text) && TransferResult(cx, TransferSet(*model, format, text));
  if (!TransferResult(cx, TransferGet(*model, format, &text))) return false;
  JS::RootedString result(cx, JS_NewUCStringCopyN(cx, text.data(), text.size()));
  if (!result) return false;
  args.rval().setString(result); return true;
}
#define METHOD(fn, method) \
  bool fn(JSContext* cx, unsigned argc, JS::Value* vp) { \
    return Call(cx, JS::CallArgsFromVp(argc, vp), method); \
  }
METHOD(ClearData, Clear)
METHOD(GetData, Get)
METHOD(SetData, Set)
METHOD(SetDragImage, DragImage)
#undef METHOD
}
extern const JSFunctionSpec kTransferMethods[] = {
  JS_FN("clearData", ClearData, 0, JSPROP_ENUMERATE),
  JS_FN("getData", GetData, 1, JSPROP_ENUMERATE),
  JS_FN("setData", SetData, 2, JSPROP_ENUMERATE),
  JS_FN("setDragImage", SetDragImage, 3, JSPROP_ENUMERATE), JS_FS_END};
}  // namespace ruyijs::interaction
