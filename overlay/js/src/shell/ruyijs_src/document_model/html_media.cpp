// HTMLMediaElement's common surface.  The node and attribute state remains in
// DocumentModel; this adapter supplies Firefox's inert media defaults used by
// Audio() and by HTML audio/video elements.
#include "document_model/html_control.h"
#include "document_model/model_hyperlink.h"
#include "browser_environment/environment.h"
#include "interaction/realm.h"
#include "runtime/web_kit.h"
#include "runtime/runtime.h"
#include "domain/web_media/can_play.h"
#include "js/CharacterEncoding.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include <cmath>

namespace ruyijs::document_model {
namespace {
enum Field { Src, CurrentSrc, Preload, Autoplay, Loop, Controls, Muted,
             DefaultMuted, Volume, CurrentTime, Paused, Ended, ReadyState, NetworkState };
struct Spec { const char* name; const char16_t* attr; int kind; };
constexpr Spec kFields[] = {
  {"src",u"src",0},{"currentSrc",u"src",1},{"preload",u"preload",0},
  {"autoplay",u"autoplay",2},{"loop",u"loop",2},{"controls",u"controls",2},
  {"muted",u"muted",2},{"defaultMuted",u"defaultmuted",2},
  {"volume",nullptr,3},{"currentTime",nullptr,4},{"paused",nullptr,5},
  {"ended",nullptr,6},{"readyState",nullptr,7},{"networkState",nullptr,8}
};
bool Resolved(JSContext* cx, State& state, DomStringView raw, JS::MutableHandleValue out) {
  JS::RootedValue base(cx), source(cx);
  if (!state.environment.URL(&base) || !Value(cx, raw, &source)) return false;
  JS::RootedString baseString(cx, base.toString()), sourceString(cx, source.toString());
  std::string baseText, sourceText, result;
  if (!ToUTF8(cx, baseString, &baseText) || !ToUTF8(cx, sourceString, &sourceText) ||
      !state.Check(ReadHyperlink(sourceText, baseText, HyperlinkField::Href, &result))) return false;
  JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(result.data(), result.size())));
  if (!text) return false; out.setString(text); return true;
}
bool Get(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp); int field=FunctionSlot(args); const Spec& s=kFields[field];
  Wrapper* entry=ControlReceiver(cx,args.thisv(),"HTMLMediaElement"); if(!entry)return false;
  State& state=*entry->owner; std::optional<DomString> text;
  if(s.kind<=2) {
    if(!state.Check(state.model.GetAttribute(entry->key,s.attr,&text)))return false;
    if(s.kind==2) args.rval().setBoolean(text.has_value());
    else if(field==Src || field==CurrentSrc) return text ? Resolved(cx,state,DomStringView(*text),args.rval()) : (args.rval().setString(JS_NewStringCopyZ(cx,"")),true);
    else if(text) return Value(cx,DomStringView(*text),args.rval());
    else return Value(cx,field==Preload?DomStringView(u"auto"):DomStringView(),args.rval());
    return true;
  }
  switch(s.kind) {
    case 3: args.rval().setNumber(1); break; case 4: args.rval().setNumber(0); break;
    case 5: args.rval().setBoolean(true); break; case 6: args.rval().setBoolean(false); break;
    case 7: case 8: args.rval().setInt32(0); break;
  }
  return true;
}
bool Set(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp); const Spec& s=kFields[FunctionSlot(args)];
  Wrapper* entry=ControlReceiver(cx,args.thisv(),"HTMLMediaElement"); if(!entry)return false;
  State& state=*entry->owner; DomString text;
  if(s.kind==2) {
    if(!JS::ToBoolean(args.get(0))) return state.Check(state.model.RemoveAttribute(entry->key,s.attr));
    return state.Check(state.model.SetAttribute(entry->key,s.attr,u""));
  }
  if(s.kind==3) { double value; if(!JS::ToNumber(cx,args.get(0),&value))return false;
    if(!std::isfinite(value)||value<0||value>1)return WebKit::FailKind(cx,"volume is outside the valid range",JSEXN_RANGEERR);
    args.rval().setUndefined(); return true;
  }
  if(s.kind==4) { double value; if(!JS::ToNumber(cx,args.get(0),&value))return false; args.rval().setUndefined(); return true; }
  if(!ControlString(cx,args.get(0),&text))return false;
  args.rval().setUndefined(); return state.Check(state.model.SetAttribute(entry->key,s.attr,text));
}
bool VoidMethod(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp); if(!ControlReceiver(cx,args.thisv(),"HTMLMediaElement"))return false;
  args.rval().setUndefined(); return true;
}
// The answer comes from the web_media domain's decoder rules (Firefox
// DecoderTraits); this adapter only converts the DOMString argument.
bool CanPlayType(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp); if(!ControlReceiver(cx,args.thisv(),"HTMLMediaElement"))return false;
  if(!argc) return WebKit::Fail(cx,"HTMLMediaElement.canPlayType: At least 1 argument required, but only 0 passed");
  JS::RootedString type(cx,JS::ToString(cx,args[0])); std::string text;
  if(!type || !ToUTF8(cx,type,&text)) return false;
  const char* answer=domain::web_media::CanPlayText(domain::web_media::CanPlayType(text));
  JSString* result=JS_NewStringCopyZ(cx,answer); if(!result)return false;
  args.rval().setString(result); return true;
}
bool Play(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp); if(!ControlReceiver(cx,args.thisv(),"HTMLMediaElement"))return false;
  JS::RootedObject promise(cx,JS::NewPromiseObject(cx,nullptr)); if(!promise)return false;
  if(!JS::ResolvePromise(cx,promise,JS::UndefinedHandleValue))return false; args.rval().setObject(*promise); return true;
}
bool MediaKeysGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp); Wrapper* entry=ControlReceiver(cx,args.thisv(),"HTMLMediaElement");
  if(!entry)return false; JS::RootedValue value(cx,entry->mediaKeys);
  if(!JS_WrapValue(cx,&value))return false; args.rval().set(value); return true;
}
bool HandlerGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp); Wrapper* entry=ControlReceiver(cx,args.thisv(),"HTMLMediaElement");
  if(!entry)return false;
  const int index=FunctionSlot(args);
  {
    JSAutoRealm realm(cx,entry->owner->page);
    JS::RootedObject object(cx,entry->object);
    JS::RootedValue map(cx,JS::GetReservedSlot(object,2));
    args.rval().setNull();
    if(map.isObject()) { JS::RootedObject values(cx,&map.toObject());
      if(!JS_GetElement(cx,values,index,args.rval()))return false;
    }
    if(args.rval().isUndefined())args.rval().setNull();
  }
  return JS_WrapValue(cx,args.rval());
}
bool HandlerSet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp); Wrapper* entry=ControlReceiver(cx,args.thisv(),"HTMLMediaElement");
  if(!entry)return false; State& state=*entry->owner; const int index=FunctionSlot(args);
  JSAutoRealm realm(cx,state.page); JS::RootedObject object(cx,entry->object), map(cx);
  JS::RootedValue desired(cx,args.get(0)); if(!JS_WrapValue(cx,&desired))return false;
  if(!desired.isObject() || !JS::IsCallable(&desired.toObject()))desired.setNull();
  JS::RootedValue stored(cx,JS::GetReservedSlot(object,2)); map=stored.isObject()?&stored.toObject():JS_NewPlainObject(cx);
  if(!map)return false; JS::SetReservedSlot(object,2,JS::ObjectValue(*map)); JS::RootedValue previous(cx);
  if(!JS_GetElement(cx,map,index,&previous))return false; JS::RootedObject prototype(cx,state.interaction.prototype());
  JS::RootedValue function(cx),receiver(cx,JS::ObjectValue(*object)),ignored(cx);
  const char* typeName=index==0?"encrypted":"waitingforkey"; JS::RootedString type(cx,JS_NewStringCopyZ(cx,typeName));
  if(!type)return false; JS::RootedValueArray<2> values(cx); values[0].setString(type);
  for(int operation=0;operation<2;++operation){values[1].set(operation?desired:previous); if(!values[1].isObject())continue;
    if(!JS_GetProperty(cx,prototype,operation?"addEventListener":"removeEventListener",&function) ||
       !JS::Call(cx,receiver,function,values,&ignored))return false;}
  if(!JS_SetElement(cx,map,index,desired))return false; args.rval().setUndefined(); return true;
}
bool SetMediaKeys(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp); Wrapper* entry=ControlReceiver(cx,args.thisv(),"HTMLMediaElement"); if(!entry)return false;
  State& state=*entry->owner; JS::RootedValue value(cx,args.get(0));
  if(!value.isNull() && (!state.mediaKeysCheck || !state.mediaKeysCheck(state.mediaKeysCheckContext,value)))
    return WebKit::Fail(cx,"setMediaKeys: value is not a MediaKeys object");
  entry->mediaKeys.set(value);
  JS::RootedObject promise(cx,JS::NewPromiseObject(cx,nullptr)); if(!promise)return false;
  if(!JS::ResolvePromise(cx,promise,JS::UndefinedHandleValue))return false; args.rval().setObject(*promise); return true;
}
// The element's single TextTrackList: created by WebMedia in the element's
// realm on first use and held by the element's own media map, so it lives
// exactly as long as the element and is the same object on every read.
bool TrackList(JSContext* cx, Wrapper* entry, JS::MutableHandleObject list) {
  State& state=*entry->owner;
  if(!state.textTracks.list || !state.textTracks.append)
    return WebKit::Fail(cx,"HTMLMediaElement text tracks are not bound");
  JSAutoRealm realm(cx,state.page);
  JS::RootedObject object(cx,entry->object), map(cx);
  JS::RootedValue stored(cx,JS::GetReservedSlot(object,2));
  map=stored.isObject()?&stored.toObject():JS_NewPlainObject(cx);
  if(!map)return false;
  JS::SetReservedSlot(object,2,JS::ObjectValue(*map));
  bool cached=false; JS::RootedValue value(cx);
  if(!JS_HasOwnProperty(cx,map,"textTracks",&cached))return false;
  if(cached) {
    if(!JS_GetProperty(cx,map,"textTracks",&value))return false;
    list.set(&value.toObject()); return true;
  }
  if(!state.textTracks.list(cx,list))return false;
  value.setObject(*list);
  return JS_DefineProperty(cx,map,"textTracks",value,JSPROP_READONLY|JSPROP_PERMANENT);
}
bool TextTracksGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  Wrapper* entry=ControlReceiver(cx,args.thisv(),"HTMLMediaElement","get textTracks");
  JS::RootedObject list(cx);
  if(!entry || !TrackList(cx,entry,&list))return false;
  args.rval().setObject(*list); return JS_WrapValue(cx,args.rval());
}
bool OptionalString(JSContext* cx,const JS::CallArgs& args,unsigned index,JS::MutableHandleString out) {
  out.set(args.length()>index && !args[index].isUndefined()?JS::ToString(cx,args[index]):JS_GetEmptyString(cx));
  return out!=nullptr;
}
bool AddTextTrack(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  Wrapper* entry=ControlReceiver(cx,args.thisv(),"HTMLMediaElement","addTextTrack");
  if(!entry)return false;
  if(!argc)return TypeError(cx,"HTMLMediaElement.addTextTrack: At least 1 argument required, but only 0 passed");
  JS::RootedString kind(cx,JS::ToString(cx,args[0])), label(cx), language(cx);
  std::string text;
  if(!kind || !ToUTF8(cx,kind,&text))return false;
  static constexpr const char* kKinds[]={"subtitles","captions","descriptions","chapters","metadata"};
  const char* known=nullptr;
  for(const char* candidate:kKinds) if(text==candidate) known=candidate;
  if(!known)
    return TypeError(cx,("HTMLMediaElement.addTextTrack: '"+text+
        "' (value of argument 1) is not a valid value for enumeration TextTrackKind.").c_str());
  JS::RootedObject list(cx), track(cx);
  if(!OptionalString(cx,args,1,&label) || !OptionalString(cx,args,2,&language) ||
     !TrackList(cx,entry,&list) || !entry->owner->textTracks.append(cx,list,known,label,language,&track))
    return false;
  args.rval().setObject(*track); return JS_WrapValue(cx,args.rval());
}
}
bool InstallMediaElement(State& state) {
  for(size_t i=0;i<std::size(kFields);++i) {
    bool writable=kFields[i].kind<=4 && i!=CurrentSrc;
    if(!state.Accessor("HTMLMediaElement",kFields[i].name,Get,int32_t(i),writable?Set:nullptr))return false;
  }
  if(!state.Accessor("HTMLMediaElement","mediaKeys",MediaKeysGet,0,nullptr) ||
     !state.Accessor("HTMLMediaElement","onencrypted",HandlerGet,0,HandlerSet) ||
     !state.Accessor("HTMLMediaElement","onwaitingforkey",HandlerGet,1,HandlerSet) ||
     !state.Method("HTMLMediaElement","setMediaKeys",SetMediaKeys,1,0) ||
     !state.Accessor("HTMLMediaElement","textTracks",TextTracksGet,0,nullptr) ||
     !state.Method("HTMLMediaElement","addTextTrack",AddTextTrack,1,0)) return false;
  return state.Method("HTMLMediaElement","load",VoidMethod,0,0) &&
    state.Method("HTMLMediaElement","canPlayType",CanPlayType,1,0) &&
    state.Method("HTMLMediaElement","fastSeek",VoidMethod,1,0) &&
    state.Method("HTMLMediaElement","pause",VoidMethod,0,0) &&
    state.Method("HTMLMediaElement","play",Play,0,0);
}
}
