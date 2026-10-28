#include "web_indexeddb/state.h"
#include "js/CallAndConstruct.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
namespace ruyijs::web_indexeddb {
// Payload events use the one Event factory and Interaction dispatch. D65
// records the missing IDBVersionChangeEvent subtype/trusted payload factory.
bool VersionEvent(State& s,JS::HandleObject target,const char* type,uint64_t old,
                  uint64_t version,bool remove) {
  JSContext* cx=s.cx;JSAutoRealm realm(cx,s.kit.page);
  JS::RootedString name(cx,JS_NewStringCopyZ(cx,type));
  JS::RootedValue argument(cx),next(cx,remove?JS::NullValue():JS::DoubleValue(double(version)));
  JS::RootedObject event(cx);
  if (!name) return false;
  argument.setString(name);
  unsigned flags=JSPROP_READONLY|JSPROP_ENUMERATE;
  return JS::Construct(cx,s.eventCtor,JS::HandleValueArray(argument),&event) &&
         JS_DefineProperty(cx,event,"oldVersion",double(old),flags) &&
         JS_DefineProperty(cx,event,"newVersion",next,flags) &&
         s.events.Dispatch(target,event);
}
}
