#include "web_audio/node_fields.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::web_audio {
bool InstallNodeSurface(State& s) {
  WebKit& kit=s.kit;
  for(size_t i=0;i<kNodeFieldCount;++i) {
    const auto& f=kNodeFields[i];
    WebFace face(s.cx);face.prototype=s.nodePrototypes.get()[f.node];
    if(!kit.Accessor(face,kNodeDefs[f.node].name,f.name,NodeFieldGet,
                     f.writable?NodeFieldSet:nullptr,int32_t(i))) return false;
  }
  struct Method {int32_t node;const char* name;unsigned length;int32_t op;};
  constexpr Method methods[]={
    {1,"getByteFrequencyData",1,0},{1,"getByteTimeDomainData",1,1},
    {1,"getFloatFrequencyData",1,2},{1,"getFloatTimeDomainData",1,3},
    {2,"getFrequencyResponse",3,4},{8,"getFrequencyResponse",3,5},
    {9,"setPosition",3,6},{9,"setOrientation",3,7}};
  for(const auto& m:methods) {
    WebFace face(s.cx);face.prototype=s.nodePrototypes.get()[m.node];
    if(!kit.Method(face,kNodeDefs[m.node].name,m.name,NodeExtraMethod,m.length,m.op))
      return false;
  }
  if(!kit.Method(s.oscillator,"OscillatorNode","setPeriodicWave",NodeExtraMethod,1,8) ||
     !kit.Accessor(s.scheduled,"AudioScheduledSourceNode","onended",AudioEventGet,
                   AudioEventSet,kNodeEndedSlot)) return false;
  WebFace source(s.cx);source.prototype=s.nodePrototypes.get()[14];
  if(!kit.Method(source,"AudioBufferSourceNode","start",NodeMethod,0,kGenericNode))
    return false;
  // Required constructor arity comes from Firefox's generated bindings.
  for(int32_t i=0;i<kNodeDefCount;++i) {
    const int32_t length=i==10?0:(i==8 || i==15 || i==16 || i==17 || i==19)?2:1;
    JS::RootedObject ctor(s.cx,s.nodeConstructors.get()[i]);
    if(!JS_DefineProperty(s.cx,ctor,"length",length,JSPROP_READONLY)) return false;
  }
  for(WebFace* face:{&s.offline,&s.oscillator,&s.buffer,&s.periodic})
    if(!JS_DefineProperty(s.cx,face->constructor,"length",1,JSPROP_READONLY)) return false;
  return true;
}
}
