#include "web_audio/node_fields.h"
#include "js/MapAndSet.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "js/Symbol.h"

namespace ruyijs::web_audio {
const JSClass kParamMapClass={"AudioParamMap",JSCLASS_HAS_RESERVED_SLOTS(1)};
namespace {
bool MapMember(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  const int32_t op=WebKit::Slot(args);
  JS::RootedObject self(cx);
  if(!WebKit::Check(cx,args.thisv(),&kParamMapClass,&self,
                   "AudioParamMap member called on incompatible receiver")) return false;
  JS::RootedObject map(cx,&JS::GetReservedSlot(self,0).toObject());
  if(op==0) {args.rval().setNumber(JS::MapSize(cx,map));return true;}
  if(op==1) return JS::MapGet(cx,map,args.get(0),args.rval());
  if(op==2) {
    bool has=false;if(!JS::MapHas(cx,map,args.get(0),&has)) return false;
    args.rval().setBoolean(has);return true;
  }
  if(op==3) return JS::MapEntries(cx,map,args.rval());
  if(op==4) return JS::MapKeys(cx,map,args.rval());
  if(op==5) return JS::MapValues(cx,map,args.rval());
  if(!JS::MapForEach(cx,map,args.get(0),args.get(1))) return false;
  args.rval().setUndefined();return true;
}

}
JSObject* NewParamMap(State& s) {
  JS::RootedObject object(s.cx,s.kit.New(&kParamMapClass,s.paramMap.prototype));
  JS::RootedObject map(s.cx,JS::NewMapObject(s.cx));
  if(!object || !map) return nullptr;
  JS::SetReservedSlot(object,0,JS::ObjectValue(*map));return object;
}
bool InstallAuxiliary(State& s,JS::HandleObject parent) {
  if(!s.kit.Define("AudioParamMap",&kParamMapClass,parent,WebKit::Reject,&s.paramMap) ||
     !s.kit.Accessor(s.paramMap,"AudioParamMap","size",MapMember,nullptr,0)) return false;
  const char* names[]={"get","has","entries","keys","values","forEach"};
  for(int32_t i=0;i<6;++i)
    if(!s.kit.Method(s.paramMap,"AudioParamMap",names[i],MapMember,
                    i<2 || i==5?1:0,i+1)) return false;
  JS::RootedValue entries(s.cx);
  JS::RootedId iterator(s.cx,JS::PropertyKey::Symbol(
    JS::GetWellKnownSymbol(s.cx,JS::SymbolCode::iterator)));
  return JS_GetProperty(s.cx,s.paramMap.prototype,"entries",&entries) &&
    JS_DefinePropertyById(s.cx,s.paramMap.prototype,iterator,entries,0);
}
}
