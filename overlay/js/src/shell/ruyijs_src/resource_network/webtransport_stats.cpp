#include "resource_network/webtransport.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::resource_network {
namespace {
bool Field(JSContext* cx,JS::HandleObject object,const WebTransportEvent& event,const char* name) {
  for (const auto& stat:event.statistics)if(stat.name==name) {
    JS::RootedValue value(cx,JS::DoubleValue(stat.value));return JS_DefineProperty(cx,object,name,value,JSPROP_ENUMERATE);
  }
  return true;
}
}
bool TransportStatsObject(JSContext* cx,const WebTransportEvent& event,bool stream,bool sending,JS::MutableHandleValue result) {
  JS::RootedObject object(cx,JS_NewPlainObject(cx)),datagrams(cx,JS_NewPlainObject(cx));if (!object || !datagrams)return false;
  if (stream) {
    if(sending) {for(const char* name:{"bytesWritten","bytesSent","bytesAcknowledged"})if(!Field(cx,object,event,name))return false;}
    else {for(const char* name:{"bytesReceived","bytesRead"})if(!Field(cx,object,event,name))return false;}
  } else {
    for(const char* name:{"bytesSent","bytesSentOverhead","bytesAcknowledged","packetsSent","bytesLost","packetsLost",
        "bytesReceived","packetsReceived","smoothedRtt","rttVariation","minRtt"})if(!Field(cx,object,event,name))return false;
    for (const char* name:{"droppedIncoming","expiredIncoming","expiredOutgoing","lostOutgoing"}) {
      double number=0;for (const auto& field:event.statistics)if(field.name==name)number=field.value;
      JS::RootedValue value(cx,JS::DoubleValue(number));if (!JS_DefineProperty(cx,datagrams,name,value,JSPROP_ENUMERATE))return false;
    }
    JS::RootedValue nested(cx,JS::ObjectValue(*datagrams));
    if (!JS_DefineProperty(cx,object,"datagrams",nested,JSPROP_ENUMERATE))return false;
    JS::RootedValue empty(cx,JS::NullValue()),capacity(cx,JS::BooleanValue(false));
    if (!JS_DefineProperty(cx,object,"estimatedSendRate",empty,JSPROP_ENUMERATE) || !JS_DefineProperty(cx,object,"atSendCapacity",capacity,JSPROP_ENUMERATE))return false;
  }
  result.setObject(*object);return true;
}
}
