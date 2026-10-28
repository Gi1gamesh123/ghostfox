// The Request constructor and its argument normalization. This file owns the
// one conversion from (input, init) to a RequestDraft; the fetch entry calls
// the same RequestInput, so both paths produce one resource request description
// and one NetLog record. The object layout lives in request_object.cpp.
#include "resource_network/state.h"
#include "resource_network/request.h"
#include "resource_network/abort.h"
#include "resource_network/body.h"
#include "runtime/page_helpers.h"
#include "runtime/native_constructor_trace.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"

namespace ruyijs::resource_network {
namespace {
// The input Request's body moves to the new request, which is what makes
// fetch(request) and new Request(request) leave the source consumed. Only an
// absent body transfers nothing; a present zero-byte body is still consumed.
bool TransferBody(State& state, JS::HandleObject source, RequestDraft* draft) {
  BodyRecord* body = BodyOf(source);
  if (!body || body->nullBody) return true;
  JS::RootedValue receiver(state.cx, JS::ObjectValue(*source));
  if (body->used || BodyLocked(BodyHolderOf(receiver)))
    return Error(state.cx, "Body has already been consumed or is locked");
  draft->body = body->bytes;
  draft->kind = body->kind;
  draft->nullBody = body->nullBody;
  return BeginBodyConsume(state.cx, receiver);
}
}  // namespace

bool RequestInput(State& state, JS::HandleValue input, JS::HandleValue init, RequestDraft* draft,
                  JS::MutableHandleValue signal) {
  JSContext* cx = state.cx;
  signal.setNull();
  JS::RootedObject source(cx);
  if (input.isObject() && JS::GetClass(&input.toObject()) == &kRequestClass)
    source = &input.toObject();
  std::string text;
  if (!source && !EncodeBodyText(cx, input, &text)) return false;
  RequestInitFields converted;
  JS::RootedValue supplied_signal(cx);
  if (!RequestInitInput(state, init, &converted, &supplied_signal)) return false;
  if (source) {
    if (!ReadRequest(state, source, draft, signal)) return false;
  } else if (!RequestURL(state, text, &draft->url)) return false;
  if (!ApplyRequestInit(state, converted, draft)) return false;
  if (converted.present & InitSignal) signal.set(supplied_signal);
  return !converted.values.nullBody || TransferBody(state, source, draft);
}

bool RequestCreate(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  State& state = *FunctionOwner(args);
  if (!args.isConstructing()) return Error(cx, "Request requires new");
  if (!argc) return Error(cx, "Request requires an input");
  state.Observe("Request", "constructor", int(ApiAccess::Construct));
  JS::RootedObject prototype(cx, state.requestPrototype);
  JS::RootedObject target(cx, &args.newTarget().toObject());
  JS::RootedValue desired(cx);
  if (!JS_GetProperty(cx, target, "prototype", &desired)) return false;
  if (desired.isObject()) prototype = &desired.toObject();
  RequestDraft draft;
  JS::RootedValue signal(cx);
  if (!RequestInput(state, args.get(0), args.get(1), &draft, &signal)) return false;
  JS::RootedObject object(cx);
  if (!RequestNew(state, prototype, draft, signal, &object)) return false;
  args.rval().setObject(*object);
  return trace.Finish(true);
}

bool InstallRequest(State& state, PageHelpers& helpers) {
  JSContext* cx = state.cx;
  JS::RootedObject prototype(cx, JS_NewPlainObject(cx));
  JS::RootedObject constructor(
      cx, NativeFunction(state, RequestCreate, "Request", 1, JSFUN_CONSTRUCTOR));
  if (!prototype || !constructor ||
      !JS_DefineProperty(cx, constructor, "prototype", prototype,
                         JSPROP_READONLY | JSPROP_PERMANENT) ||
      !JS_DefineProperty(cx, prototype, "constructor", constructor, 0) ||
      !Tag(cx, prototype, "Request")) return false;
  state.requestPrototype = prototype;
  return InstallRequestObject(state, helpers, prototype) &&
         helpers.Constructor("Request", constructor) &&
         JS_DefineProperty(cx, state.page, "Request", constructor, 0);
}
}  // namespace ruyijs::resource_network
