// Populate the original FormData list from already-normalized Body values.
#include "web_platform/body_factories.h"
#include "web_platform/form_data_internal.h"
#include "web_platform/form_query.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::web_platform {
bool BodyFormAppend(JSContext* cx,JS::HandleObject form,JS::HandleValue name,
                     JS::HandleValue value) {
  JS::RootedObject local(cx,js::CheckedUnwrapStatic(form));
  if (!local || JS::GetClass(local)!=&kFormDataClass || !name.isString())
    return WebKit::Fail(cx,"Native Body FormData entry requires a form and string name");
  JS::RootedObject file(cx,value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr);
  if (!value.isString() && (!file || JS::GetClass(file)!=&kFileClass))
    return WebKit::Fail(cx,"Native Body FormData entry requires a string or File");
  JSAutoRealm realm(cx,local);
  JS::RootedValue key(cx,name),item(cx,value);
  JS::RootedObject list(cx),pair(cx);
  uint32_t length=0;
  return JS_WrapValue(cx,&key) && JS_WrapValue(cx,&item) && FormDataList(cx,local,&list) &&
      form_data_internal::NewPair(cx,key,item,&pair) &&
      form_data_internal::Length(cx,list,&length) &&
      form_data_internal::Push(cx,list,length,pair);
}
bool BodyFormUrlencoded(JSContext* cx,JS::HandleObject form,const std::string& bytes) {
  JS::RootedObject local(cx,js::CheckedUnwrapStatic(form));
  if (!local || JS::GetClass(local)!=&kFormDataClass)
    return WebKit::Fail(cx,"Native Body form parser requires FormData");
  JSAutoRealm realm(cx,local);
  std::vector<std::pair<std::string,std::string>> entries;
  if (!ParseFormQuery(bytes,&entries)) {JS_ReportOutOfMemory(cx);return false;}
  for (const auto& entry:entries) {
    JS::RootedValue name(cx),value(cx);
    if (!Value(cx,entry.first,&name) || !Value(cx,entry.second,&value) ||
        !BodyFormAppend(cx,local,name,value)) return false;
  }
  return true;
}
}
