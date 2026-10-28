#include "interaction/data_transfer.h"
#include "js/Conversions.h"

namespace ruyijs::interaction {
namespace {
bool Length(JSContext* cx, unsigned argc, JS::Value* vp) {
  const auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject object(cx);
  if (!TransferListReceiver(cx, args, &object)) return false;
  ObserveTransfer(cx, "DataTransferItemList", "length", ApiAccess::Read);
  args.rval().setInt32(0); return true;
}
bool ListCall(JSContext* cx, const JS::CallArgs& args, unsigned method) {
  JS::RootedObject object(cx);
  if (!TransferListReceiver(cx, args, &object)) return false;
  const char* names[] = {"add", "remove", "clear"};
  ObserveTransfer(cx, "DataTransferItemList", names[method], ApiAccess::Call);
  if (method < 2 && !args.length()) return Error(cx, "TypeError", "Item list method needs an argument");
  args.rval().setUndefined();
  if (method == 2) return true;
  if (method == 0) {
    // One-argument file overload is a null placeholder; two arguments use
    // string conversion but still create no items in the public tool.
    if (args.length() > 1) {
      std::string text, type;
      if (!TransferString(cx, args[0], &text) || !TransferString(cx, args[1], &type)) return false;
    }
    args.rval().setNull(); return true;
  }
  uint32_t index;
  if (!JS::ToUint32(cx, args[0], &index)) return false;
  State* state = RealmState(cx);
  if (!state || !state->exceptionReporter)
    return Error(cx, "TypeError", "Event exception reporter is not installed");
  return state->exceptionReporter(state->exceptionContext, "IndexSizeError", "Item list is empty");
}
bool ListAdd(JSContext* cx, unsigned argc, JS::Value* vp) {
  return ListCall(cx, JS::CallArgsFromVp(argc, vp), 0);
}
bool ListRemove(JSContext* cx, unsigned argc, JS::Value* vp) {
  return ListCall(cx, JS::CallArgsFromVp(argc, vp), 1);
}
bool ListClear(JSContext* cx, unsigned argc, JS::Value* vp) {
  return ListCall(cx, JS::CallArgsFromVp(argc, vp), 2);
}
// There is no public factory producing an Item: add returns null, files throws.
// Prototype borrowing must still reject forged receivers, not invent payloads.
bool ItemMember(JSContext* cx, unsigned, JS::Value*) {
  return Error(cx, "TypeError", "DataTransferItem receiver is incompatible");
}
}
extern const JSPropertySpec kTransferListProperties[] = {
  JS_PSG("length", Length, JSPROP_ENUMERATE), JS_PS_END};
extern const JSFunctionSpec kTransferListMethods[] = {
  JS_FN("add", ListAdd, 1, JSPROP_ENUMERATE),
  JS_FN("remove", ListRemove, 1, JSPROP_ENUMERATE),
  JS_FN("clear", ListClear, 0, JSPROP_ENUMERATE), JS_FS_END};
extern const JSPropertySpec kTransferItemProperties[] = {
  JS_PSG("kind", ItemMember, JSPROP_ENUMERATE),
  JS_PSG("type", ItemMember, JSPROP_ENUMERATE), JS_PS_END};
extern const JSFunctionSpec kTransferItemMethods[] = {
  JS_FN("getAsString", ItemMember, 1, JSPROP_ENUMERATE),
  JS_FN("getAsFile", ItemMember, 0, JSPROP_ENUMERATE), JS_FS_END};
}  // namespace ruyijs::interaction
