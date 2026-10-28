#include "web_styling/page_descriptors.h"
#include "js/Symbol.h"
#include "js/Wrapper.h"
#include <cstdio>
#include <iterator>
namespace ruyijs::web_styling {
namespace {
struct PageProperty {const char* idl;const char* css;};
constexpr PageProperty pageProperties[]={
  {"pageOrientation","page-orientation"},{"page-orientation","page-orientation"},{"size","size"},
  {"marginBottom","margin-bottom"},{"margin-bottom","margin-bottom"},
  {"marginLeft","margin-left"},{"margin-left","margin-left"},
  {"marginRight","margin-right"},{"margin-right","margin-right"},
  {"marginTop","margin-top"},{"margin-top","margin-top"},{"margin","margin"},
  {"marginBlock","margin-block"},{"margin-block","margin-block"},
  {"marginInline","margin-inline"},{"margin-inline","margin-inline"},
  {"paddingBlock","padding-block"},{"padding-block","padding-block"},
  {"paddingInline","padding-inline"},{"padding-inline","padding-inline"}};
bool PageNamed(JSContext* cx,unsigned argc,JS::Value* vp,bool setter) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject self(cx);
  const auto& property=pageProperties[WebKit::Slot(args)];
  self=args.thisv().isObject()?js::CheckedUnwrapStatic(&args.thisv().toObject()):nullptr;
  if(!self||JS::GetClass(self)!=&kPageDescriptorsClass) {
    char message[160];std::snprintf(message,sizeof(message),
      "'%s %s' called on an object that does not implement interface CSSPageDescriptors.",
      setter?"set":"get",property.idl);
    return WebKit::Fail(cx,message);
  }
  return CallPageDeclaration(cx,setter?document_model::StyleMember::NamedSet:document_model::StyleMember::NamedGet,
                         self,args,property.css,property.idl);
}
bool PageNamedGet(JSContext* cx,unsigned argc,JS::Value* vp) {return PageNamed(cx,argc,vp,false);}
bool PageNamedSet(JSContext* cx,unsigned argc,JS::Value* vp) {return PageNamed(cx,argc,vp,true);}
}
bool InstallPageDescriptors(State& state) {
  auto& kit=state.kit;JS::RootedObject base(kit.cx);
  if(!JS_GetPrototype(kit.cx,state.declarationPrototype,&base)||!base||
      !kit.Define("CSSPageDescriptors",&kPageDescriptorsClass,base,WebKit::Reject,&state.pageDescriptors)) return false;
  JS::RootedValue constructor(kit.cx);
  if(!JS_GetProperty(kit.cx,base,"constructor",&constructor)||!constructor.isObject()) return false;
  JS::RootedObject parent(kit.cx,&constructor.toObject());
  if(!JS_SetPrototype(kit.cx,state.pageDescriptors.constructor,parent)) return false;
  for(size_t i=0;i<std::size(pageProperties);++i)
    if(!kit.Accessor(state.pageDescriptors,"CSSPageDescriptors",pageProperties[i].idl,PageNamedGet,PageNamedSet,int32_t(i)))
      return false;
  if(!OrderConstructor(kit,state.pageDescriptors)) return false;
  // Symbols retain IDL order: constructor, iterator, toStringTag.
  JS::RootedId tag(kit.cx,JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(kit.cx,JS::SymbolCode::toStringTag)));
  JS::RootedValue tagValue(kit.cx);
  JS::ObjectOpResult removed;
  if(!JS_GetPropertyById(kit.cx,state.pageDescriptors.prototype,tag,&tagValue)||
      !JS_DeletePropertyById(kit.cx,state.pageDescriptors.prototype,tag,removed)||!removed.ok()) return false;
  JS::RootedObject array(kit.cx,JS::GetRealmArrayPrototype(kit.cx));JS::RootedValue values(kit.cx);
  JS::RootedId iterator(kit.cx,JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(kit.cx,JS::SymbolCode::iterator)));
  return array&&JS_GetProperty(kit.cx,array,"values",&values)&&
    JS_DefinePropertyById(kit.cx,state.pageDescriptors.prototype,iterator,values,0)&&
    JS_DefinePropertyById(kit.cx,state.pageDescriptors.prototype,tag,tagValue,JSPROP_READONLY);
}
}
