#include "runtime/host_bridge.h"
#include "runtime/typed_decode.h"
#include "jsapi.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs {
bool HostBridge::ExposeData(JS::HandleString name, JS::HandleValue encoded) {
  JSAutoRealm realm(cx_, page_);
  JS::RootedValue localName(cx_, JS::StringValue(name)), input(cx_, encoded), value(cx_);
  JS::RootedId id(cx_);
  if (!JS_WrapValue(cx_, &localName) || !JS_WrapValue(cx_, &input)) return false;
  JS::RootedString key(cx_, localName.toString());
  if (!JS_StringToId(cx_, key, &id) || !DecodeTyped(cx_, input, &value)) return false;
  if (data_)
    return JS_DefinePropertyById(cx_, data_, id, value, JSPROP_ENUMERATE) &&
           Record(Kind::Data, name, nullptr, false, encoded);

  JS::RootedObject lexical(cx_, JS_GlobalLexicalEnvironment(page_));
  bool occupied = false;
  if (!JS_HasOwnProperty(cx_, lexical, "__ruyijs__", &occupied)) return false;
  if (!occupied && !JS_HasOwnProperty(cx_, page_, "__ruyijs__", &occupied)) return false;
  if (occupied) {
    JS_ReportErrorASCII(cx_, "Hidden data namespace __ruyijs__ is already declared");
    return false;
  }
  JS::RootedObject dictionary(cx_, JS_NewObjectWithGivenProto(cx_, nullptr, nullptr));
  if (!dictionary) return false;
  JS::RootedObject space(cx_, JS_NewObjectWithGivenProto(cx_, nullptr, nullptr));
  if (!space || !JS_DefinePropertyById(cx_, dictionary, id, value, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx_, space, "data", dictionary, JSPROP_ENUMERATE) ||
      !JS_FreezeObject(cx_, space)) return false;
  // Commit only after decoding and constructing the entire channel. The lexical
  // environment is engine-owned and never reachable through Window reflection.
  if (!JS_DefineProperty(cx_, lexical, "__ruyijs__", space,
                         JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return false;
  data_ = dictionary;
  return Record(Kind::Data, name, nullptr, false, encoded);
}
}
