// WebMedia's indexed projection; track state remains in its traced list slots.
#include "web_media/image_decoder.h"
#include "js/PropertyDescriptor.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "jsfriendapi.h"
namespace ruyijs::web_media {
const JSClass kImageTrackListClass=PROXY_CLASS_DEF("ImageTrackList",JSCLASS_HAS_RESERVED_SLOTS(ILSlots));
namespace {
char listFamily;
bool Index(JSContext* cx,JS::HandleId id,uint32_t* index,bool* indexed) {
  *indexed=id.isInt();if(*indexed){*index=uint32_t(id.toInt());return true;}
  if(!id.isString())return true;
  JSLinearString* text=JS_EnsureLinearString(cx,id.toString());if(!text)return false;
  *indexed=js::StringIsArrayIndex(text,index)&&*index!=UINT32_MAX;return true;
}
class Handler final:public js::NurseryAllocableProxyHandler {
 public:
  constexpr Handler():NurseryAllocableProxyHandler(&listFamily){}
  bool getOwnPropertyDescriptor(JSContext* cx,JS::HandleObject proxy,JS::HandleId id,
      JS::MutableHandle<mozilla::Maybe<JS::PropertyDescriptor>> desc)const override {
    uint32_t index=0;bool indexed;if(!Index(cx,id,&index,&indexed))return false;
    if(indexed){desc.reset();JS::RootedObject track(cx,ImageObject(proxy,ILTrack));
      if(index||!track)return true;JS::RootedValue value(cx,JS::ObjectValue(*track));
      desc.set(mozilla::Some(JS::PropertyDescriptor::Data(value,
          {JS::PropertyAttribute::Enumerable,JS::PropertyAttribute::Configurable})));return true;}
    JS::RootedObject expando(cx,&js::GetProxyPrivate(proxy).toObject());
    return JS_GetOwnPropertyDescriptorById(cx,expando,id,desc);
  }
  bool defineProperty(JSContext* cx,JS::HandleObject proxy,JS::HandleId id,
      JS::Handle<JS::PropertyDescriptor> desc,JS::ObjectOpResult& result)const override {
    uint32_t index=0;bool indexed;if(!Index(cx,id,&index,&indexed))return false;
    if(indexed)return result.failReadOnly();
    JS::RootedObject expando(cx,&js::GetProxyPrivate(proxy).toObject());
    return JS_DefinePropertyById(cx,expando,id,desc,result);
  }
  bool ownPropertyKeys(JSContext* cx,JS::HandleObject proxy,JS::MutableHandleIdVector keys)const override {
    if(ImageObject(proxy,ILTrack)&&!keys.append(JS::PropertyKey::Int(0)))return false;
    JS::RootedObject expando(cx,&js::GetProxyPrivate(proxy).toObject());JS::RootedIdVector extra(cx);
    return js::GetPropertyKeys(cx,expando,JSITER_OWNONLY|JSITER_HIDDEN|JSITER_SYMBOLS,&extra)&&keys.appendAll(extra);
  }
  bool delete_(JSContext* cx,JS::HandleObject proxy,JS::HandleId id,JS::ObjectOpResult& result)const override {
    uint32_t index=0;bool indexed;if(!Index(cx,id,&index,&indexed))return false;
    if(indexed)return index||!ImageObject(proxy,ILTrack)?result.succeed():result.failCantDelete();
    JS::RootedObject expando(cx,&js::GetProxyPrivate(proxy).toObject());return JS_DeletePropertyById(cx,expando,id,result);
  }
  bool getPrototypeIfOrdinary(JSContext* cx,JS::HandleObject proxy,bool* ordinary,JS::MutableHandleObject proto)const override {
    *ordinary=true;return JS_GetPrototype(cx,proxy,proto);
  }
  bool preventExtensions(JSContext*,JS::HandleObject,JS::ObjectOpResult& result)const override{return result.failCantPreventExtensions();}
  bool isExtensible(JSContext*,JS::HandleObject,bool* extensible)const override{*extensible=true;return true;}
  const char* className(JSContext*,JS::HandleObject)const override{return "ImageTrackList";}
};
const Handler handler;
}
const js::BaseProxyHandler* ImageTrackListHandler(){return &handler;}
}
