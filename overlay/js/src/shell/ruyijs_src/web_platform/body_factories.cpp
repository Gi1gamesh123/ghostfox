// Native access to original WebPlatform result factories; no public lookup.
#include "web_platform/body_factories.h"
#include "web_platform/state.h"
#include "runtime/page_reference.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/String.h"
#include "js/Wrapper.h"

namespace ruyijs::web_platform {
namespace {
enum Slot { Owner,BlobPrototype,FilePrototype,FormPrototype,Slots };
const JSClass kFactories={"WebPlatform Body factories",JSCLASS_HAS_RESERVED_SLOTS(Slots)};
JSObject* ObjectSlot(JSObject* object,unsigned slot) {
  auto value=JS::GetReservedSlot(object,slot);
  return value.isObject()?&value.toObject():nullptr;
}
State* OwnerState(JSContext* cx,JS::HandleObject input,JS::MutableHandleObject record) {
  record.set(js::CheckedUnwrapStatic(input));
  if (!record || JS::GetClass(record)!=&kFactories) {
    WebKit::Fail(cx,"WebPlatform Body factory record is invalid");return nullptr;
  }
  JS::RootedObject original(cx,ObjectSlot(record,Owner));
  if (!original || !JS_IsNativeFunction(original,BlobConstructor)) {
    WebKit::Fail(cx,"WebPlatform Body factory owner is unavailable");return nullptr;
  }
  auto owner=js::GetFunctionNativeReserved(original,0);
  auto kind=js::GetFunctionNativeReserved(original,1);
  if (!owner.isDouble() || !owner.toPrivate() || !kind.isInt32() || kind.toInt32()!=0) {
    WebKit::Fail(cx,"WebPlatform Body factory owner is invalid");return nullptr;
  }
  auto* state=static_cast<State*>(owner.toPrivate());
  if (state->cx!=cx) {
    WebKit::Fail(cx,"WebPlatform Body factory context is invalid");return nullptr;
  }
  return state;
}
}
bool InstallBodyFactories(State& state) {
  JSContext* cx=state.cx;
  if (!state.blob.constructor || !state.blob.prototype || !state.file.prototype ||
      !state.formData.prototype) return WebKit::Fail(cx,"WebPlatform Body factories are not installed");
  JS::RootedObject record(cx,JS_NewObjectWithGivenProto(cx,&kFactories,nullptr));
  if (!record) return false;
  JS::SetReservedSlot(record,Owner,JS::ObjectValue(*state.blob.constructor));
  JS::SetReservedSlot(record,BlobPrototype,JS::ObjectValue(*state.blob.prototype));
  JS::SetReservedSlot(record,FilePrototype,JS::ObjectValue(*state.file.prototype));
  JS::SetReservedSlot(record,FormPrototype,JS::ObjectValue(*state.formData.prototype));
  JS::RootedValue value(cx,JS::ObjectValue(*record));
  return WritePageReference(state.kit.page,PageReference::PlatformBodyFactories,value);
}
bool BodyFactories(JSContext* cx,JS::HandleObject global,JS::MutableHandleObject result) {
  JS::RootedObject local(cx,js::CheckedUnwrapStatic(global));
  JS::RootedValue value(cx);
  if (!local || !ReadPageReference(local,PageReference::PlatformBodyFactories,&value) ||
      !value.isObject() || JS::GetClass(&value.toObject())!=&kFactories)
    return WebKit::Fail(cx,"WebPlatform Body factories are unavailable");
  result.set(&value.toObject());
  return JS_WrapObject(cx,result);
}
bool BodyBlob(JSContext* cx,JS::HandleObject factories,const std::string& bytes,
               const std::string& type,JS::MutableHandleObject result) {
  JS::RootedObject record(cx);
  State* state=OwnerState(cx,factories,&record);
  if (!state) return false;
  {
    JSAutoRealm realm(cx,record);
    JS::RootedObject prototype(cx,ObjectSlot(record,BlobPrototype));
    JS::RootedString input(cx,JS_NewStringCopyN(cx,type.data(),type.size()));
    if (!input) return false;
    JS::RootedValue value(cx,JS::StringValue(input));std::string normalized;
    if (!NormalizeType(cx,value,&normalized)) return false;
    result.set(MakeBlob(*state,bytes,normalized,&kBlobClass,prototype));
    if (!result) return false;
  }
  return JS_WrapObject(cx,result);
}
bool BodyFile(JSContext* cx,JS::HandleObject factories,const std::string& bytes,
               const std::string& type,const std::string& name,JS::MutableHandleObject result) {
  JS::RootedObject record(cx);
  State* state=OwnerState(cx,factories,&record);
  if (!state) return false;
  {
    JSAutoRealm realm(cx,record);
    JS::RootedObject prototype(cx,ObjectSlot(record,FilePrototype));
    JS::RootedString input(cx,JS_NewStringCopyN(cx,type.data(),type.size()));
    if (!input) return false;
    JS::RootedValue value(cx,JS::StringValue(input));std::string normalized;
    if (!NormalizeType(cx,value,&normalized)) return false;
    result.set(MakeFile(*state,bytes,normalized,name,prototype));
    if (!result) return false;
  }
  return JS_WrapObject(cx,result);
}
bool BodyFormData(JSContext* cx,JS::HandleObject factories,JS::MutableHandleObject result) {
  JS::RootedObject record(cx);
  State* state=OwnerState(cx,factories,&record);
  if (!state) return false;
  {
    JSAutoRealm realm(cx,record);
    JS::RootedObject prototype(cx,ObjectSlot(record,FormPrototype));
    if (!MakeFormData(*state,prototype,result)) return false;
  }
  return JS_WrapObject(cx,result);
}
}
