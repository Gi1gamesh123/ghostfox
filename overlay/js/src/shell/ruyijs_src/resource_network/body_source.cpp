// Response-specific source identity; the original stream keeps all stream state.
#include "resource_network/readable_stream.h"
#include "resource_network/state.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
bool StreamDisturbed(JSObject* stream) {
  if (BodyRecord* body=StreamBody(stream)) return body->used;
  return JS::GetReservedSlot(stream,SDisturbed).toBoolean();
}
bool BodyUsed(JSObject* holder) {
  const BodyRecord* body=HolderBody(holder);
  if (!body) return false;
  if (body->used) return true;
  JSObject* stream=SlotObject(holder,HStream);
  return stream && StreamDisturbed(stream);
}
const BodyRecord* BodyBytesOf(JSObject* holder) {
  const BodyRecord* body=HolderBody(holder);
  return body && body->source==BodySource::NativeBytesAlias?
      HolderBody(SlotObject(holder,HNativeSource)):body;
}
bool SetResponseStream(JSContext* cx,JS::HandleObject holder,JS::HandleObject source) {
  JSAutoRealm realm(cx,holder);
  JS::RootedObject stream(cx,source),native(cx,SlotObject(source,SHolder));
  if (!JS_WrapObject(cx,&stream) || (native && !JS_WrapObject(cx,&native))) return false;
  BodyRecord* body=HolderBody(holder);
  body->source=native?BodySource::NativeBytesAlias:BodySource::ScriptStream;
  body->nullBody=false;body->kind=BodyKind::Bytes;
  JS::SetReservedSlot(holder,HStream,JS::ObjectValue(*stream));
  JS::SetReservedSlot(holder,HNativeSource,JS::ObjectOrNullValue(native));return true;
}
}
