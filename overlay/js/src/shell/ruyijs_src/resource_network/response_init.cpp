// Original Response normalization; stream recognition is Response-specific.
#include "resource_network/state.h"
#include "resource_network/readable_stream.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include <cmath>
#include <memory>
#include <new>
namespace ruyijs::resource_network {
// The one argument-normalization path behind new Response and every static.
bool CreateResponse(State& state, const JS::CallArgs& args, int32_t forced_status, const char* type,
            const HeaderPairs& extra, const char* body_text, JS::MutableHandleObject result) {
  JSContext* cx = state.cx;
  JS::RootedObject prototype(cx, state.responsePrototype);
  if (args.isConstructing()) {
    JS::RootedObject target(cx, &args.newTarget().toObject());
    JS::RootedValue desired(cx);
    if (!JS_GetProperty(cx, target, "prototype", &desired)) return false;
    if (desired.isObject()) prototype = &desired.toObject();
  }
  std::unique_ptr<BodyRecord> body(new (std::nothrow) BodyRecord());
  if (!body) { JS_ReportOutOfMemory(cx); return false; }
  JS::RootedObject source(cx,forced_status==0?StreamReceiver(args.get(0)):nullptr);
  if (source) body->nullBody=false;
  if (body_text) {
    body->bytes = body_text;
    body->nullBody = false;
  } else if (forced_status == 0 && !source && !BodyInput(state, args.get(0), body.get())) return false;
  int32_t status = source?200:forced_status;
  JS::RootedString status_text(cx, JS_NewStringCopyZ(cx, ""));
  HeaderPairs headers = extra;
  if (!status_text) return false;
  if (forced_status == 0 && args.get(1).isObject()) {
    JS::RootedObject init(cx, &args[1].toObject());
    JS::RootedValue value(cx);
    if (source && (!JS_GetProperty(cx,init,"headers",&value) || !HeaderPairsArgument(cx,value,&headers))) return false;
    if (!JS_GetProperty(cx, init, "status", &value)) return false;
    if (!value.isUndefined()) {
      double number = 0;
      if (!JS::ToNumber(cx, value, &number)) return false;
      if (source) {
        status=std::isfinite(number)?int32_t(std::fmod(std::trunc(number),65536.0)):0;
        if (status<0) status+=65536;
      } else {
        if (number<200 || number>599) return Error(cx,"Response status must be 200-599",true);
        status=int32_t(number);
      }
    }
    if (!JS_GetProperty(cx, init, "statusText", &value)) return false;
    if (!value.isUndefined()) {
      status_text = JS::ToString(cx, value);
      if (!status_text) return false;
    }
    if (!source && (!JS_GetProperty(cx,init,"headers",&value) || !HeaderPairsArgument(cx,value,&headers))) return false;
  }
  if (source && (status<200 || status>599)) return Error(cx,"Response constructor: Invalid response status code.",true);
  if (!source && status==0) status=200;
  if (!body->nullBody && (status == 204 || status == 205 || status == 304))
    return Error(cx,source?"Response constructor: Response body is given with a null body status.":"Response with a null body status cannot have a body");
  if (source && (!JS::GetReservedSlot(source,SReader).isUndefined() || StreamDisturbed(source)))
    return Error(cx,"Response constructor: Body has already been consumed.");
  JS::RootedObject header_object(cx);
  JS::RootedString url(cx, JS_NewStringCopyZ(cx, ""));
  if (!url || !HeaderObject(state, headers, &header_object)) return false;
  if (!ResponseNew(state, prototype, body.get(), header_object, status, status_text, url, type,
                   false, result)) return false;
  body.release();
  JS::RootedObject holder(cx,SlotObject(result,0));
  return !source || SetResponseStream(cx,holder,source);
}

}
