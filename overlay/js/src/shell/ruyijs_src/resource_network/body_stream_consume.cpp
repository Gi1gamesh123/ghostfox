// Public Body outcome policy over the shared typed acquisition target.
#include "resource_network/body_acquire_internal.h"
#include "resource_network/body_result.h"
#include "resource_network/abort.h"
#include "resource_network/state.h"
#include "web_platform/body_factories.h"
#include "js/Promise.h"
#include "js/GlobalObject.h"
#include "js/Realm.h"
#include "js/String.h"
#include <memory>
#include <new>

namespace ruyijs::resource_network {
namespace {
enum ConsumerSlot { PPromise,PKind,PMime,PPlatform,PFactories,PSlots };
void FinalizeConsumer(JS::GCContext*,JSObject* object) {
  auto value=JS::GetReservedSlot(object,PMime);
  if (!value.isUndefined()) delete static_cast<std::string*>(value.toPrivate());
}
const JSClassOps kConsumerOps={nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,FinalizeConsumer};
const JSClass kConsumer={"Resource Body consumer",JSCLASS_HAS_RESERVED_SLOTS(PSlots)|
    JSCLASS_FOREGROUND_FINALIZE,&kConsumerOps};
bool RejectPending(JSContext* cx,JS::HandleObject promise) {
  JS::RootedValue error(cx);
  if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx,&error)) return false;
  JS_ClearPendingException(cx);
  return JS::RejectPromise(cx,promise,error);
}
bool Finish(JSContext* cx,JS::HandleObject target,JS::HandleObject holder,
              BodyOutcome outcome,JS::HandleValue reason) {
  JSAutoRealm realm(cx,target);
  JS::RootedObject promise(cx,SlotObject(target,PPromise)),factories(cx,SlotObject(target,PFactories));
  JS::RootedObject platform(cx,SlotObject(target,PPlatform));
  JS::RootedValue value(cx,reason);
  State* state=StreamFactoryState(cx,factories);
  if (!state) return false;
  if (outcome!=BodyOutcome::Complete) {
    if (outcome==BodyOutcome::Locked)
      return Error(cx,"Public Body consumer received an AfterValidation-only outcome");
    if (outcome==BodyOutcome::WrongType) {
      Error(cx,"Can't convert value to Uint8Array while consuming Body");
      return RejectPending(cx,promise);
    }
    if (outcome!=BodyOutcome::NativeFailure) {
      const bool unavailable=outcome==BodyOutcome::Unavailable;
      if (!AbortException(*state,unavailable?"InvalidStateError":"AbortError",&value,unavailable?
          "An attempt was made to use an object that is not, or is no longer, usable":"The operation was aborted. ")) return false;
    }
    return JS_WrapValue(cx,&value) && JS::RejectPromise(cx,promise,value);
  }
  const BodyRecord* body=BodyBytesOf(holder);
  const auto kind=BodyResultKind(JS::GetReservedSlot(target,PKind).toInt32());
  bool ok=false,rejected=false;
  const char* parseError=nullptr;
  const auto& full=*static_cast<const std::string*>(JS::GetReservedSlot(target,PMime).toPrivate());
  switch (kind) {
    case BodyResultKind::Text: ok=BodyTextValue(cx,body,&value);break;
    case BodyResultKind::Json: ok=BodyJsonValue(cx,body,&value,&rejected);break;
    case BodyResultKind::ArrayBuffer: ok=BodyArrayBufferValue(cx,body,&value);break;
    case BodyResultKind::Bytes: ok=BodyBytesValue(cx,body,&value);break;
    case BodyResultKind::Blob: ok=BodyBlobValue(cx,platform,body,full,&value);break;
    case BodyResultKind::FormData: ok=BodyFormDataValue(cx,platform,body,full,&value,&parseError);break;
  }
  if (!ok) return RejectPending(cx,promise);
  if (parseError) {Error(cx,parseError);return RejectPending(cx,promise);}
  return rejected?JS::RejectPromise(cx,promise,value):JS::ResolvePromise(cx,promise,value);
}
const BodyAcquireSteps kSteps={Finish};
}
bool ConsumeStreamBody(JSContext* cx,const JS::CallArgs& args,BodyResultKind kind) {
  JS::RootedObject holder(cx,BodyHolderOf(args.thisv())),promise(cx);
  JS::RootedObject factories(cx,SlotObject(holder,HFactories));
  State* state=StreamFactoryState(cx,factories);
  if (!state) return false;
  JS::RootedObject global(cx,JS::GetNonCCWObjectGlobal(holder));
  BodyRecord* body=HolderBody(holder);
  // Original StartConsuming fails before creating the receiver-owned consumer.
  // Binding converts that initial failure into a method-realm rejected Promise.
  if (body->source==BodySource::ScriptStream && state->callbackEntry &&
      !state->callbackEntry(state->callbackEntryContext,global)) {
    body->used=true;
    JS::RootedValue error(cx);
    return AbortException(*FunctionOwner(args),"InvalidStateError",&error,
        "An attempt was made to use an object that is not, or is no longer, usable") &&
        Rejected(cx,error,args.rval());
  }
  {
    JSAutoRealm realm(cx,holder);
    JS::RootedObject platform(cx),target(cx);
    std::string full;
    if ((kind==BodyResultKind::Blob || kind==BodyResultKind::FormData) &&
        (!MimeType(*state,args.thisv(),&full) || !web_platform::BodyFactories(cx,state->page,&platform))) return false;
    std::unique_ptr<std::string> mime(new(std::nothrow) std::string(std::move(full)));
    if (!mime) {JS_ReportOutOfMemory(cx);return false;}
    promise=JS::NewPromiseObject(cx,nullptr);
    target=JS_NewObjectWithGivenProto(cx,&kConsumer,nullptr);
    if (!promise || !target) return false;
    JS::SetReservedSlot(target,PPromise,JS::ObjectValue(*promise));
    JS::SetReservedSlot(target,PKind,JS::Int32Value(int(kind)));
    JS::SetReservedSlot(target,PMime,JS::PrivateValue(mime.release()));
    JS::SetReservedSlot(target,PPlatform,JS::ObjectOrNullValue(platform));
    JS::SetReservedSlot(target,PFactories,JS::ObjectValue(*factories));
    if (!AcquireBody(cx,holder,BodyAcquireMode::PublicConsume,target,kSteps)) return false;
  }
  args.rval().setObject(*promise);return JS_WrapValue(cx,args.rval());
}
}
