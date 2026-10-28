#include "runtime/http_environment_ports.h"
#include "runtime/browsing_context.h"
#include "runtime/runtime.h"
#include "execution_scheduling/realm.h"
#include "runtime/resource_timing_ports.h"
#include "js/PropertyAndElement.h"
#include "js/Array.h"

namespace ruyijs {
namespace {
bool DocumentURL(BrowsingContext& target,std::string* out) {
  JSAutoRealm realm(target.cx,target.page);
  JS::RootedValue value(target.cx);
  if(!target.environment->URL(&value)||!value.isString())return false;
  JS::RootedString text(target.cx,value.toString());
  if(!ToUTF8(target.cx,text,out))return false;
  if(target.parent&&(*out=="about:blank"||*out=="about:srcdoc"))return DocumentURL(*target.parent,out);
  return true;
}
bool Environment(void* opaque,resource_network::HttpContext* out) {
  auto& target=*static_cast<BrowsingContext*>(opaque);
  if(!DocumentURL(target,&out->source))return false;
  auto* top=&target;while(top->parent)top=top->parent;
  if(!DocumentURL(*top,&out->site))return false;
  JSAutoRealm realm(target.cx,target.page);
  JS::RootedValue snapshot(target.cx),group(target.cx),ua(target.cx);
  if(!target.environment->Describe(&snapshot))return false;
  JS::RootedObject object(target.cx,&snapshot.toObject());
  if(!JS_GetProperty(target.cx,object,"navigator",&group)||!group.isObject())return false;
  object=&group.toObject();
  if(!JS_GetProperty(target.cx,object,"userAgent",&ua)||!ua.isString())return false;
  JS::RootedString text(target.cx,ua.toString());
  if(!ToUTF8(target.cx,text,&out->userAgent))return false;
  JS::RootedValue languages(target.cx),item(target.cx);
  if(!JS_GetProperty(target.cx,object,"languages",&languages)||!languages.isObject())return false;
  JS::RootedObject list(target.cx,&languages.toObject());uint32_t length=0;
  if(!JS::GetArrayLength(target.cx,list,&length))return false;
  for(uint32_t i=0;i<length;++i) {
    if(!JS_GetElement(target.cx,list,i,&item)||!item.isString())return false;
    text=item.toString();std::string value;
    if(!ToUTF8(target.cx,text,&value))return false;
    out->languages.push_back(std::move(value));
  }
  return true;
}
bool CookieDocument(void* opaque,JS::HandleValue receiver,std::string* url) {
  auto& target=*static_cast<BrowsingContext*>(opaque);
  document_model::NodeSource source,current;
  if(!target.document->ResolveNodeSource(receiver,&source))return false;
  JS::RootedValue doc(target.cx,JS::ObjectValue(*source.owner->document()));
  if(!source.owner->ResolveNodeSource(doc,&current))return false;
  if(source.key.document!=current.key.document||source.key.node!=current.key.node){url->clear();return true;}
  auto* top=&target;while(top->parent)top=top->parent;
  auto* owner=source.owner==top->document.get()?top:top->frames->FindDocument(*source.owner);
  if(!owner){url->clear();return true;}
  return DocumentURL(*owner,url);
}
double Clock(void* opaque) {
  auto snapshot=static_cast<execution_scheduling::Realm*>(opaque)->Inspect();
  return (snapshot.epoch+snapshot.now)/1000.0;
}
}
void BindHttpEnvironment(BrowsingContext& target,resource_network::Realm& network,
    execution_scheduling::Realm& scheduling) {
  network.SetHttpEnvironment(Environment,CookieDocument,&target,Clock,&scheduling);
  target.scheduling=&scheduling;
  BindResourceTiming(target,network,scheduling);
}
}
