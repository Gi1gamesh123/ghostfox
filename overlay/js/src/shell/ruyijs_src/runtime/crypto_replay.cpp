#include "runtime/crypto_replay.h"
#include "runtime/resource_schema.h"
#include "web_crypto/random_replay.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs {
bool CryptoReplayPlan::Configure(JSContext* cx, JS::HandleObject request, bool frozen,
                                 JS::MutableHandleValue result) {
  if (frozen) return resource_wire::Error(cx, "Crypto replay configuration is closed after execution");
  if (!resource_wire::Fields(cx, request, {"id","op","documents"})) return false;
  JS::RootedValue value(cx);
  bool array=false;
  if (!JS_GetProperty(cx,request,"documents",&value) || !JS::IsArrayObject(cx,value,&array)) return false;
  if (!array) return resource_wire::Error(cx,"Crypto replay documents must be an array");
  JS::RootedObject documents(cx,&value.toObject());
  uint32_t length=0;
  if (!JS::GetArrayLength(cx,documents,&length)) return false;
  if (length>32) return resource_wire::Error(cx,"Crypto replay exceeds 32 documents",true);
  std::vector<std::unique_ptr<Entry>> candidate;
  size_t total=0;
  for(uint32_t index=0;index<length;++index) {
    JS::RootedObject entry(cx), calls(cx), tape(cx);
    if(!JS_GetElement(cx,documents,index,&value) || !resource_wire::Object(cx,value,&entry) ||
       !resource_wire::Fields(cx,entry,{"url","values"})) return false;
    std::string url;
    if(!resource_wire::Property(cx,entry,"url",&url,8192) ||
       !JS_GetProperty(cx,entry,"values",&value) || !JS::IsArrayObject(cx,value,&array)) return false;
    if(url.empty()||url.find(':')==std::string::npos||url.find('\0')!=std::string::npos||!array)
      return resource_wire::Error(cx,"Crypto replay requires an exact absolute URL and values array");
    calls=&value.toObject();
    uint32_t count=0;
    if(!JS::GetArrayLength(cx,calls,&count)) return false;
    if(count>128)return resource_wire::Error(cx,"Crypto replay exceeds 128 calls per document",true);
    for(uint32_t n=0;n<count;++n) {
      if(!JS_GetElement(cx,calls,n,&value))return false;
      if(value.isObject()) {
        JS::RootedObject item(cx,&value.toObject());
        std::string method;
        if(!resource_wire::Fields(cx,item,{"method","value"})||
           !resource_wire::Property(cx,item,"method",&method,32)||
           !JS_GetProperty(cx,item,"value",&value))return false;
        if(method!="randomUUID")return resource_wire::Error(cx,"Unknown recorded Crypto method");
      }
      if(!value.isString())return resource_wire::Error(cx,"Crypto input must be hexadecimal text");
      total+=JS_GetStringLength(value.toString());
      if(total>8*1024*1024)return resource_wire::Error(cx,"Crypto replay exceeds 4 MiB",true);
    }
    if(!web_crypto::CreateRandomReplay(cx,calls,&tape))return false;
    candidate.push_back(std::make_unique<Entry>(cx,std::move(url),tape));
  }
  entries_.swap(candidate);
  result.setBoolean(true);
  return true;
}
bool CryptoReplayPlan::Bind(JSContext* cx, JS::HandleObject page, const std::string& url) {
  if(entries_.empty())return true;
  bool known=false;
  for(const auto& entry:entries_) {
    if(entry->url!=url)continue;
    known=true;
    if(entry->bound)continue;
    JS::RootedObject tape(cx,entry->tape);
    if(!web_crypto::AttachRandomReplay(cx,page,tape))return false;
    entry->bound=true;
    return true;
  }
  if(known)return resource_wire::Error(cx,"Recorded Crypto document occurrences exhausted");
  JS::RootedObject none(cx);
  return web_crypto::AttachRandomReplay(cx,page,none);
}
bool CryptoReplayPlan::Status(JSContext* cx, JS::MutableHandleValue result) {
  JS::RootedObject rows(cx,JS::NewArrayObject(cx,entries_.size()));
  if(!rows)return false;
  for(uint32_t index=0;index<entries_.size();++index) {
    const auto& entry=*entries_[index];
    JS::RootedObject tape(cx,entry.tape);
    JS::RootedValue value(cx);
    if(!web_crypto::RandomReplayStatus(cx,tape,&value))return false;
    JS::RootedObject row(cx,&value.toObject());
    JS::RootedString url(cx,JS_NewStringCopyN(cx,entry.url.data(),entry.url.size()));
    JS::RootedValue bound(cx,JS::BooleanValue(entry.bound));
    if(!url||!JS_DefineProperty(cx,row,"url",url,JSPROP_ENUMERATE)||
       !JS_DefineProperty(cx,row,"bound",bound,JSPROP_ENUMERATE)||
       !JS_SetElement(cx,rows,index,value))return false;
  }
  result.setObject(*rows);
  return true;
}
}
