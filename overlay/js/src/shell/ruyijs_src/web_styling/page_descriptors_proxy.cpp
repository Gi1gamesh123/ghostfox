// Native indexed view: every value is read from the original rule declaration.
#include "web_styling/page_descriptors.h"
#include "js/PropertyDescriptor.h"
#include "js/String.h"
#include "jsfriendapi.h"
namespace ruyijs::web_styling {
const JSClass kPageDescriptorsClass=PROXY_CLASS_DEF("CSSPageDescriptors",JSCLASS_HAS_RESERVED_SLOTS(1));
namespace {
char pageDescriptorsFamily;
bool PageIndex(JSContext* cx,JS::HandleId id,uint32_t* index,bool* indexed) {
  *indexed=id.isInt();if(*indexed) {*index=uint32_t(id.toInt());return true;}
  if(!id.isString()) return true;
  auto text=JS_EnsureLinearString(cx,id.toString());if(!text) return false;
  *indexed=js::StringIsArrayIndex(text,index)&&*index!=UINT32_MAX;return true;
}
class PageDescriptorsProxy final:public js::NurseryAllocableProxyHandler {
 public:
  constexpr PageDescriptorsProxy():NurseryAllocableProxyHandler(&pageDescriptorsFamily) {}
  bool getOwnPropertyDescriptor(JSContext* cx,JS::HandleObject proxy,JS::HandleId id,
      JS::MutableHandle<mozilla::Maybe<JS::PropertyDescriptor>> desc) const override {
    uint32_t index=0;bool indexed;
    if(!PageIndex(cx,id,&index,&indexed)) return false;
    if(indexed) {
      desc.reset();const auto& rows=PageDeclarationRows(cx,proxy);
      if(index>=rows.size()) return true;
      const auto& name=rows[index].name;
      JS::RootedString text(cx,JS_NewStringCopyUTF8N(cx,JS::UTF8Chars(name.data(),name.size())));
      if(!text) return false;
      JS::RootedValue value(cx,JS::StringValue(text));
      desc.set(mozilla::Some(JS::PropertyDescriptor::Data(value,
        {JS::PropertyAttribute::Enumerable,JS::PropertyAttribute::Configurable})));
      return true;
    }
    JS::RootedObject storage(cx,DeclarationStorage(proxy));
    return JS_GetOwnPropertyDescriptorById(cx,storage,id,desc);
  }
  bool defineProperty(JSContext* cx,JS::HandleObject proxy,JS::HandleId id,
      JS::Handle<JS::PropertyDescriptor> desc,JS::ObjectOpResult& result) const override {
    uint32_t index=0;bool indexed;
    if(!PageIndex(cx,id,&index,&indexed)) return false;
    if(indexed) return result.failCantRedefineProp();
    JS::RootedObject storage(cx,DeclarationStorage(proxy));
    return JS_DefinePropertyById(cx,storage,id,desc,result);
  }
  bool ownPropertyKeys(JSContext* cx,JS::HandleObject proxy,JS::MutableHandleIdVector keys) const override {
    const auto length=PageDeclarationRows(cx,proxy).size();
    for(uint32_t i=0;i<length;++i) {
      JS::RootedId id(cx);if(!JS_IndexToId(cx,i,&id)||!keys.append(id)) return false;
    }
    JS::RootedObject storage(cx,DeclarationStorage(proxy));JS::RootedIdVector extra(cx);
    if(!js::GetPropertyKeys(cx,storage,JSITER_OWNONLY|JSITER_HIDDEN|JSITER_SYMBOLS,&extra)) return false;
    if(!keys.appendAll(extra)) {JS_ReportOutOfMemory(cx);return false;}
    return true;
  }
  bool delete_(JSContext* cx,JS::HandleObject proxy,JS::HandleId id,JS::ObjectOpResult& result) const override {
    uint32_t index=0;bool indexed;
    if(!PageIndex(cx,id,&index,&indexed)) return false;
    if(indexed) {
      return index<PageDeclarationRows(cx,proxy).size()?result.failCantDelete():result.succeed();
    }
    JS::RootedObject storage(cx,DeclarationStorage(proxy));return JS_DeletePropertyById(cx,storage,id,result);
  }
  bool getPrototypeIfOrdinary(JSContext* cx,JS::HandleObject proxy,bool* ordinary,
                              JS::MutableHandleObject proto) const override {
    *ordinary=true;return JS_GetPrototype(cx,proxy,proto);
  }
  bool preventExtensions(JSContext*,JS::HandleObject,JS::ObjectOpResult& result) const override {
    return result.failCantPreventExtensions();
  }
  bool isExtensible(JSContext*,JS::HandleObject,bool* out) const override {*out=true;return true;}
  const char* className(JSContext*,JS::HandleObject) const override {return "CSSPageDescriptors";}
};
const PageDescriptorsProxy pageDescriptorsHandler;
}
const js::BaseProxyHandler* PageDescriptorsHandler() {return &pageDescriptorsHandler;}
}
