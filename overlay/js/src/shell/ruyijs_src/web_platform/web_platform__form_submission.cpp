#include "web_platform/form_submission.h"
#include "web_platform/form_data_internal.h"
#include "web_platform/request_body.h"
#include "web_platform/realm.h"
#include "js/Realm.h"
#include "js/String.h"

namespace ruyijs::web_platform {
bool Realm::CreateFormData(const std::vector<FormField>& fields, JS::MutableHandleObject out) {
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.formData.prototype);
  JS::RootedObject prototype(s.cx, s.formData.prototype), form(s.cx), list(s.cx);
  if (!MakeFormData(s, prototype, &form) || !FormDataList(s.cx, form, &list) ||
      !PopulateFormData(s, form, fields)) return false;
  out.set(form);
  return true;
}
bool Realm::EncodeFormData(JS::HandleObject form, const std::string& encoding,
                           std::string* bytes, std::string* contentType) {
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.formData.prototype);
  JS::RootedValue self(s.cx, JS::ObjectValue(*form));
  JS::RootedObject local(s.cx), list(s.cx);
  if (!FormDataBrand(s.cx, self, &local)) return false;
  bytes->clear();
  if (encoding == "multipart/form-data") {
    RequestBody body;
    if (!ReadFormRequestBody(s.cx, local, &body)) return false;
    if (body.tooLarge) return WebKit::Fail(s.cx, "Form body exceeds the transport limit");
    *bytes = std::move(body.bytes); *contentType = std::move(body.contentType);
    return true;
  }
  const bool plain = encoding == "text/plain";
  *contentType = plain ? "text/plain" : "application/x-www-form-urlencoded";
  uint32_t count = 0;
  if (!FormDataList(s.cx, local, &list) || !form_data_internal::Length(s.cx, list, &count)) return false;
  for (uint32_t i = 0; i < count; ++i) {
    JS::RootedObject pair(s.cx);
    JS::RootedValue name(s.cx), value(s.cx);
    std::string key, text;
    if (!form_data_internal::Entry(s.cx, list, i, &pair) ||
        !form_data_internal::Pair(s.cx, pair, &name, &value)) return false;
    if (value.isObject()) value = JS::GetReservedSlot(&value.toObject(), kFileSlotName);
    if (!ReadRequestText(s.cx, name, &key) || !ReadRequestText(s.cx, value, &text)) return false;
    key = NormalizeFormLines(key); text = NormalizeFormLines(text);
    if (plain) *bytes += key + "=" + text + "\r\n";
    else {
      if (i) *bytes += '&';
      *bytes += FormEncode(key) + "=" + FormEncode(text);
    }
    if (bytes->size() > kRequestBodyLimit) return WebKit::Fail(s.cx, "Form body exceeds the transport limit");
  }
  return true;
}
}
