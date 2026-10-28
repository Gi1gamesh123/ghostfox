// One public installer under the existing WebPlatform realm.
#include "web_platform/compression_stream.h"
#include "runtime/page_helpers.h"
#include "runtime/page_reference.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/String.h"
#include "js/Symbol.h"
#include "js/Wrapper.h"

namespace ruyijs::web_platform {
namespace {
bool Get(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);const unsigned field=unsigned(WebKit::Slot(args));
  const bool decompress=field>=CSSlots;const unsigned slot=field%CSSlots;
  JS::RootedObject object(cx,args.thisv().isObject()?js::CheckedUnwrapStatic(&args.thisv().toObject()):nullptr);
  if (!object || JS::GetClass(object)!=(decompress?&kDecompressionClass:&kCompressionClass))
    return WebKit::Fail(cx,decompress?(slot==CSReadable?
        "'get readable' called on an object that does not implement interface DecompressionStream.":
        "'get writable' called on an object that does not implement interface DecompressionStream."):(slot==CSReadable?
        "'get readable' called on an object that does not implement interface CompressionStream.":
        "'get writable' called on an object that does not implement interface CompressionStream."));
  args.rval().set(JS::GetReservedSlot(object,slot));return JS_WrapValue(cx,args.rval());
}
bool InstallOne(State& state,bool decompress) {
  JSContext* cx=state.cx;auto& kit=state.kit;
  const char* name=decompress?"DecompressionStream":"CompressionStream";
  auto& face=decompress?state.decompressionStream:state.compressionStream;
  JS::RootedObject parent(cx,JS::GetRealmObjectPrototype(cx));
  if (!parent || !kit.Define(name,decompress?&kDecompressionClass:&kCompressionClass,parent,
      decompress?DecompressionConstructor:CompressionConstructor,&face,1)) return false;
  JS::RootedId tag(cx,JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx,JS::SymbolCode::toStringTag)));
  const unsigned offset=decompress?CSSlots:0;
  if (!JS_DeleteProperty(cx,face.prototype,"constructor") || !JS_DeletePropertyById(cx,face.prototype,tag) ||
      !kit.Accessor(face,name,"readable",Get,nullptr,offset+CSReadable) ||
      !kit.Accessor(face,name,"writable",Get,nullptr,offset+CSWritable)) return false;
  JS::RootedString label(cx,JS_NewStringCopyZ(cx,name));
  JS::RootedValue prototype(cx,JS::ObjectValue(*face.prototype));
  return label && JS_DefineProperty(cx,face.prototype,"constructor",face.constructor,0) &&
      JS_DefinePropertyById(cx,face.prototype,tag,label,JSPROP_READONLY) &&
      WritePageReference(kit.page,decompress?PageReference::PlatformDecompressionPrototype:
          PageReference::PlatformCompressionPrototype,prototype);
}
}
const JSClass kCompressionClass={"CompressionStream",JSCLASS_HAS_RESERVED_SLOTS(CSSlots)};
const JSClass kDecompressionClass={"DecompressionStream",JSCLASS_HAS_RESERVED_SLOTS(CSSlots)};
bool CompressionPrototype(JSContext* cx,const JS::CallArgs& args,bool decompress,JS::MutableHandleObject result) {
  JS::RootedObject target(cx,&args.newTarget().toObject());JS::RootedValue value(cx);
  if (!JS_GetProperty(cx,target,"prototype",&value)) return false;
  if (value.isObject()) {result.set(&value.toObject());return true;}
  JS::Realm* realm=JS::GetFunctionRealm(cx,target);if (!realm) return false;
  JS::RootedObject global(cx,JS::GetRealmGlobalOrNull(realm));
  if (!ReadPageReference(global,decompress?PageReference::PlatformDecompressionPrototype:
      PageReference::PlatformCompressionPrototype,&value) || !value.isObject())
    return WebKit::Fail(cx,"Compression target realm is not installed");
  result.set(&value.toObject());return JS_WrapObject(cx,result);
}
bool InstallCompressionStreams(State& state,PageHelpers&) {return InstallOne(state,false) && InstallOne(state,true);}
}
