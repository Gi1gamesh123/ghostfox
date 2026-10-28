#include "web_media/image_decoder.h"
#include "web_media/realm.h"
#include "runtime/page_reference.h"
#include "runtime/native_constructor_trace.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"
#include "js/Symbol.h"
namespace ruyijs::web_media {
namespace {
bool Illegal(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);NativeConstructorTrace trace(cx,args);
  return trace.Finish(WebKit::Fail(cx,"Illegal constructor."));
}
bool Begin(State& s,WebFace& face) {
  JS::RootedId tag(s.cx,JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(s.cx,JS::SymbolCode::toStringTag)));
  return JS_DeleteProperty(s.cx,face.prototype,"constructor") && JS_DeletePropertyById(s.cx,face.prototype,tag);
}
bool Finish(State& s,WebFace& face,const char* name) {
  JS::RootedString text(s.cx,JS_NewStringCopyZ(s.cx,name));
  JS::RootedId tag(s.cx,JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(s.cx,JS::SymbolCode::toStringTag)));
  return text && JS_DefineProperty(s.cx,face.prototype,"constructor",face.constructor,0) &&
      JS_DefinePropertyById(s.cx,face.prototype,tag,text,JSPROP_READONLY);
}
}
void Realm::SetImageRectPrototype(JS::HandleObject prototype) { state_->imageRectPrototype=prototype; }
bool InstallImageDecoder(State& s) {
  auto& k=s.kit;JSContext* cx=s.cx;
  JS::RootedObject parent(cx,JS::GetRealmObjectPrototype(cx));
  if(!parent || !k.Define("ImageDecoder",&kImageDecoderClass,parent,ImageDecoderConstructor,&s.imageDecoder,1) ||
     !k.Define("ImageTrackList",&kImageTrackListClass,parent,Illegal,&s.imageTrackList) ||
     !k.Define("ImageTrack",&kImageTrackClass,parent,Illegal,&s.imageTrack) ||
     !k.Define("VideoFrame",&kVideoFrameClass,parent,VideoFrameConstructor,&s.videoFrame,1))return false;
  if(!Begin(s,s.imageDecoder)||!Begin(s,s.imageTrackList)||!Begin(s,s.imageTrack)||!Begin(s,s.videoFrame))return false;
  for(unsigned i=0;i<3;++i) {
    static const char* names[]={"decode","reset","close"};
    if(!k.Method(s.imageDecoder,"ImageDecoder",names[i],ImageDecoderMethod,0,i))return false;
  }
  for(unsigned i=0;i<4;++i) {
    static const char* decoder[]={"type","complete","completed","tracks"};
    static const char* list[]={"ready","length","selectedIndex","selectedTrack"};
    static const char* track[]={"animated","frameCount","repetitionCount","selected"};
    if(!k.Accessor(s.imageDecoder,"ImageDecoder",decoder[i],ImageDecoderGet,nullptr,i) ||
       !k.Accessor(s.imageTrackList,"ImageTrackList",list[i],ImageListGet,nullptr,i) ||
       !k.Accessor(s.imageTrack,"ImageTrack",track[i],ImageTrackGet,i==3?ImageTrackSet:nullptr,i))return false;
  }
  for(unsigned i=0;i<4;++i) {
    static const char* names[]={"allocationSize","copyTo","clone","close"};
    if(!k.Method(s.videoFrame,"VideoFrame",names[i],i<2?VideoFrameCopy:VideoFrameMethod,i==1?1:0,i))return false;
  }
  for(unsigned i=0;i<10;++i) {
    static const char* names[]={"format","codedWidth","codedHeight","codedRect","visibleRect", "displayWidth","displayHeight","duration","timestamp","colorSpace"};
    if(!k.Accessor(s.videoFrame,"VideoFrame",names[i],VideoFrameGet,nullptr,i))return false;
  }
  if(!Finish(s,s.imageDecoder,"ImageDecoder") || !Finish(s,s.imageTrack,"ImageTrack") ||
      !Finish(s,s.videoFrame,"VideoFrame"))return false;
  JS::RootedValue values(cx,s.arrayValues);
  JS::RootedId iterator(cx,JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx,JS::SymbolCode::iterator)));
  if(!JS_DefineProperty(cx,s.imageTrackList.prototype,"constructor",s.imageTrackList.constructor,0) ||
      !JS_DefinePropertyById(cx,s.imageTrackList.prototype,iterator,values,0) ||
      !Finish(s,s.imageTrackList,"ImageTrackList"))return false;
  JS::RootedObject supported(cx,k.Function(ImageTypeSupported,"isTypeSupported",1,0));
  if(!supported||!JS_DefineProperty(cx,s.imageDecoder.constructor,"isTypeSupported",supported,JSPROP_ENUMERATE))return false;
  JS::RootedObject factories(cx,JS::NewArrayObject(cx,IFFactories));
  JS::RootedValue value(cx);
  if(!factories || !s.imageRectPrototype)return false;
  JS::RootedValueArray<IFFactories> all(cx);
  all[IFDecoder].setObject(*s.imageDecoder.prototype);all[IFTrackList].setObject(*s.imageTrackList.prototype);
  all[IFTrack].setObject(*s.imageTrack.prototype);all[IFFrame].setObject(*s.videoFrame.prototype);
  all[IFColor].setObject(*s.videoColorSpace.prototype);all[IFRect].setObject(*s.imageRectPrototype);all[IFException].set(s.exceptionConstructor);
  for(unsigned i=0;i<IFFactories;++i)if(!JS_SetElement(cx,factories,i,all[i]))return false;
  value.setObject(*factories);
  return WritePageReference(k.page,PageReference::MediaImageFactories,value) &&
      s.secure.Add(k.page,"ImageDecoder") && s.secure.Add(k.page,"ImageTrackList") && s.secure.Add(k.page,"ImageTrack");
}
}
