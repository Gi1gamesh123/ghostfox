#include "document_model/state.h"
#include "js/Conversions.h"
#include "js/CallAndConstruct.h"
#include "js/Exception.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
constexpr const char* names[] = {"createAttributeNS", "createCDATASection"};
bool CDATAError(State& state) {
  JSContext* cx = state.cx;
  constexpr const char* message = "Failed to execute 'createCDATASection' on 'Document': Failed to create CDATA section node";
  JS::RootedValue constructor(cx);
  if (JS_GetProperty(cx, state.page, "DOMException", &constructor) && constructor.isObject() &&
      JS::IsConstructor(&constructor.toObject())) {
    JS::RootedValueArray<2> arguments(cx);
    JS::RootedString text(cx, JS_NewStringCopyZ(cx, message));
    JS::RootedString name(cx, JS_NewStringCopyZ(cx, "NotSupportedError"));
    if (!text || !name) return false;
    arguments[0].setString(text); arguments[1].setString(name);
    JS::RootedObject error(cx);
    if (JS::Construct(cx, constructor, arguments, &error)) {
      JS::RootedValue value(cx, JS::ObjectValue(*error));
      JS_SetPendingException(cx, value); return false;
    }
    if (!JS_IsExceptionPending(cx)) return false;
  }
  if (JS_IsThrowingOutOfMemory(cx)) return false;
  JS_ClearPendingException(cx);
  return StandardError(cx, message);
}
bool CDATAString(JSContext* cx, JS::HandleValue value, DomString* text) {
  JS::RootedString converted(cx, JS::ToString(cx, value));
  if (!converted) {
    if (!JS_IsExceptionPending(cx) || JS_IsThrowingOutOfMemory(cx)) return false;
    JS_ClearPendingException(cx); text->clear(); return true;
  }
  JS::RootedValue string(cx, JS::StringValue(converted));
  return String(cx, string, text);
}
bool Create(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& function = *FunctionState(args);
  const int kind = FunctionSlot(args);
  JS::RootedObject document(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!DocumentReceiver(cx, document)) return false;
  function.Observe("Document", names[kind], int(ApiAccess::Call));
  if (argc < (kind ? 1u : 2u)) return TypeError(cx, "Document factory requires more arguments");
  DomString value;
  if (!kind) {
    DomString ignored;
    if (!String(cx, args[0], &ignored) || !String(cx, args[1], &value)) return false;
    AttributeSnapshot snapshot;
    if (!function.Check(PublicAttributeName(value, &snapshot))) return false;
    if (snapshot.name.empty()) { args.rval().setNull(); return true; }
    return DetachedAttributeFor(function, snapshot, args.rval());
  }
  if (!CDATAString(cx, args[0], &value)) return false;
  if (value.find(u"]]>") != DomString::npos)
    return CDATAError(function);
  // Public CDATA instances have the interface shape but no usable native tree
  // data. Inherited Node operations reject this unbacked object normally.
  JS::RootedObject prototype(cx, function.Find("CDATASection")->prototype);
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, nullptr, prototype));
  if (!object) return false;
  args.rval().setObject(*object); return true;
}
}
bool InstallDocumentNodes(State& state) {
  for (int i = 0; i < 2; ++i)
    if (!state.Method("Document", names[i], Create, i ? 1 : 2, i)) return false;
  return true;
}
}
