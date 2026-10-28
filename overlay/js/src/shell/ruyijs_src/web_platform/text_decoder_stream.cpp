// WebPlatform owns only its codec wrapper; original Transform owns both streams.
#include "web_platform/text_decoder_stream.h"
#include "web_platform/text_codec.h"
#include "runtime/page_helpers.h"
#include "runtime/page_reference.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/String.h"
#include "js/Symbol.h"
#include "js/Wrapper.h"

namespace ruyijs::web_platform {
namespace {
void Finalize(JS::GCContext*,JSObject* object) {
  auto value=JS::GetReservedSlot(object,DSHandle);
  if (!value.isUndefined()) CloseTextDecoder(value.toPrivate());
}
const JSClassOps kOps={nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,Finalize};
bool Get(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject object(cx,args.thisv().isObject()?js::CheckedUnwrapStatic(&args.thisv().toObject()):nullptr);
  const unsigned slot=unsigned(WebKit::Slot(args));
  if (!object || JS::GetClass(object)!=&kTextDecoderStreamClass) {
    const char* message=slot==DSReadable?"'get readable' called on an object that does not implement interface TextDecoderStream.":
        slot==DSWritable?"'get writable' called on an object that does not implement interface TextDecoderStream.":
        slot==DSEncoding?"'get encoding' called on an object that does not implement interface TextDecoderStream.":
        slot==DSFatal?"'get fatal' called on an object that does not implement interface TextDecoderStream.":
        "'get ignoreBOM' called on an object that does not implement interface TextDecoderStream.";
    return WebKit::Fail(cx,message);
  }
  args.rval().set(JS::GetReservedSlot(object,slot));return JS_WrapValue(cx,args.rval());
}
}
const JSClass kTextDecoderStreamClass={"TextDecoderStream",JSCLASS_HAS_RESERVED_SLOTS(DSSlots)|
    JSCLASS_FOREGROUND_FINALIZE,&kOps};
bool DecoderStreamPrototype(JSContext* cx,const JS::CallArgs& args,JS::MutableHandleObject result) {
  JS::RootedObject target(cx,&args.newTarget().toObject());JS::RootedValue value(cx);
  if (!JS_GetProperty(cx,target,"prototype",&value)) return false;
  if (value.isObject()) {result.set(&value.toObject());return true;}
  JS::Realm* realm=JS::GetFunctionRealm(cx,target);if (!realm) return false;
  JS::RootedObject global(cx,JS::GetRealmGlobalOrNull(realm));
  if (!ReadPageReference(global,PageReference::PlatformDecoderStreamPrototype,&value) || !value.isObject())
    return WebKit::Fail(cx,"TextDecoderStream target realm is not installed");
  result.set(&value.toObject());return JS_WrapObject(cx,result);
}
bool InstallTextDecoderStream(State& state,PageHelpers&) {
  JSContext* cx=state.cx;auto& kit=state.kit;auto& face=state.textDecoderStream;
  JS::RootedObject parent(cx,JS::GetRealmObjectPrototype(cx));
  if (!parent || !kit.Define("TextDecoderStream",&kTextDecoderStreamClass,parent,TextDecoderStreamConstructor,&face)) return false;
  // Place the WebIDL mixin attributes before constructor/tag, matching donor order.
  JS::RootedId tag(cx,JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx,JS::SymbolCode::toStringTag)));
  if (!JS_DeleteProperty(cx,face.prototype,"constructor") || !JS_DeletePropertyById(cx,face.prototype,tag) ||
      !kit.Accessor(face,"TextDecoderStream","readable",Get,nullptr,DSReadable) ||
      !kit.Accessor(face,"TextDecoderStream","writable",Get,nullptr,DSWritable) ||
      !kit.Accessor(face,"TextDecoderStream","encoding",Get,nullptr,DSEncoding) ||
      !kit.Accessor(face,"TextDecoderStream","fatal",Get,nullptr,DSFatal) ||
      !kit.Accessor(face,"TextDecoderStream","ignoreBOM",Get,nullptr,DSIgnoreBom)) return false;
  JS::RootedString name(cx,JS_NewStringCopyZ(cx,"TextDecoderStream"));
  JS::RootedValue prototype(cx,JS::ObjectValue(*face.prototype));
  return name && JS_DefineProperty(cx,face.prototype,"constructor",face.constructor,0) &&
      JS_DefinePropertyById(cx,face.prototype,tag,name,JSPROP_READONLY) &&
      WritePageReference(kit.page,PageReference::PlatformDecoderStreamPrototype,prototype);
}
}
