#include "web_indexeddb/keys.h"
#include "runtime/page_helpers.h"
#include "js/PropertyAndElement.h"
#include "js/Symbol.h"
namespace ruyijs::web_indexeddb {
namespace {
bool Define(State& s,const char* name,const JSClass* clasp,WebFace& face) {
  // Resolve lazy function metadata before prototype/statics, as Firefox does.
  face.clasp=clasp;face.prototype=JS_NewPlainObject(s.cx);
  face.constructor=s.kit.Function(WebKit::Reject,name,0,0,JSFUN_CONSTRUCTOR);
  JS::RootedValue metadata(s.cx);
  JS::RootedString tag(s.cx,JS_NewStringCopyZ(s.cx,name));
  JS::RootedId tagId(s.cx,JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(s.cx,JS::SymbolCode::toStringTag)));
  return face.prototype && face.constructor && tag &&
    JS_GetProperty(s.cx,face.constructor,"length",&metadata) &&
    JS_GetProperty(s.cx,face.constructor,"name",&metadata) &&
    JS_DefineProperty(s.cx,face.constructor,"prototype",face.prototype,JSPROP_READONLY|JSPROP_PERMANENT) &&
    JS_DefinePropertyById(s.cx,face.prototype,tagId,tag,JSPROP_READONLY) &&
    JS_DefineProperty(s.cx,s.kit.page,name,face.constructor,0) &&
    s.kit.helpers->Constructor(name,face.constructor) && s.kit.helpers->Interface(name,face.prototype);
}
bool Finish(State& s,WebFace& face) {
  return JS_DeleteProperty(s.cx,face.prototype,"constructor") &&
         JS_DefineProperty(s.cx,face.prototype,"constructor",face.constructor,0);
}
}
bool InstallKeys(State& s) {
  if(!Define(s,"IDBKeyRange",&kKeyRangeClass,s.keyRangeFace) ||
     !Define(s,"IDBRecord",&kRecordClass,s.recordFace) ||
     !s.kit.Method(s.keyRangeFace,"IDBKeyRange","includes",KeyRangeIncludes,1,0) ||
     !s.kit.Method(s.factoryFace,"IDBFactory","cmp",CompareKeys,2,0))return false;
  const char* getters[]={"lower","upper","lowerOpen","upperOpen"};
  for(int i=0;i<4;++i)
    if(!s.kit.Accessor(s.keyRangeFace,"IDBKeyRange",getters[i],KeyRangeGet,nullptr,i))return false;
  const char* fields[]={"key","primaryKey","value"};
  for(int i=0;i<3;++i)
    if(!s.kit.Accessor(s.recordFace,"IDBRecord",fields[i],RecordGet,nullptr,i))return false;
  const char* methods[]={"only","lowerBound","upperBound","bound"};
  for(int i=0;i<4;++i) {
    JS::RootedObject function(s.cx,s.kit.Function(KeyRangeCreate,methods[i],i==3?2:1,i));
    if(!function || !JS_DefineProperty(s.cx,s.keyRangeFace.constructor,methods[i],function,JSPROP_ENUMERATE) ||
       !s.kit.helpers->Member(function,"IDBKeyRange",methods[i],"call"))return false;
  }
  return Finish(s,s.keyRangeFace) && Finish(s,s.recordFace);
}
}
