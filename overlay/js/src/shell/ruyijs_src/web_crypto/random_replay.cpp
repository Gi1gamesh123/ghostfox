#include "web_crypto/random_replay.h"
#include "web_crypto/subtle.h"
#include "js/Array.h"
#include "js/ArrayBuffer.h"
#include "js/CharacterEncoding.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::web_crypto {
namespace {
enum Slot { Entries, Methods, Cursor, Mismatches, Count };
const JSClass kTape = {"Recorded Crypto inputs", JSCLASS_HAS_RESERVED_SLOTS(Count)};
unsigned Hex(unsigned char c) {
  return c >= '0' && c <= '9' ? c-'0' : c >= 'a' && c <= 'f' ? c-'a'+10
       : c >= 'A' && c <= 'F' ? c-'A'+10 : 16;
}
JSObject* Tape(JS::HandleObject crypto) {
  const auto& value = JS::GetReservedSlot(crypto, RandomReplay);
  return value.isObject() ? js::UncheckedUnwrap(&value.toObject()) : nullptr;
}
}

bool CreateRandomReplay(JSContext* cx, JS::HandleObject calls, JS::MutableHandleObject tape) {
  bool array = false;
  uint32_t length = 0;
  if (!JS::IsArrayObject(cx, calls, &array) || !array) return CryptoTypeError(cx, "Crypto replay calls must be an array");
  if (!JS::GetArrayLength(cx, calls, &length)) return false;
  if (length > 128) return CryptoTypeError(cx, "Crypto replay exceeds 128 calls");
  JS::RootedObject entries(cx, JS::NewArrayObject(cx, length));
  JS::RootedObject methods(cx, JS::NewArrayObject(cx, length));
  if (!entries || !methods) return false;
  for (uint32_t index=0; index<length; ++index) {
    JS::RootedValue value(cx);
    if (!JS_GetElement(cx, calls, index, &value)) return false;
    bool uuid=value.isObject();
    if(uuid) {
      JS::RootedObject entry(cx,&value.toObject());
      if(!JS_GetProperty(cx,entry,"value",&value))return false;
    }
    if (!value.isString()) return CryptoTypeError(cx, "Crypto replay values must be strings");
    JS::RootedString text(cx, value.toString());
    size_t chars = JS_GetStringLength(text);
    if (chars > 131072 || chars % 2 || (uuid && chars!=36)) return CryptoTypeError(cx, "Crypto replay byte length is invalid");
    JS::UniqueChars encoded = JS_EncodeStringToUTF8(cx, text);
    if (!encoded) return false;
    char compact[32];
    if(uuid) {
      size_t out=0;
      for(size_t index=0;index<chars;++index) {
        if(index==8||index==13||index==18||index==23) {
          if(encoded[index]!='-')return CryptoTypeError(cx,"Recorded UUID has invalid separators");
        } else {
          if(Hex(encoded[index])>15)return CryptoTypeError(cx,"Recorded UUID contains invalid digits");
          compact[out++]=encoded[index];
        }
      }
      if(compact[12]!='4'||(Hex(compact[16])&12)!=8)return CryptoTypeError(cx,"Recorded UUID is not version 4");
    } else for (size_t offset=0; offset<chars; ++offset)
      if (Hex(encoded[offset]) > 15) return CryptoTypeError(cx, "Crypto replay contains a non-hexadecimal digit");
    const char* digits=uuid?compact:encoded.get();
    JS::RootedObject buffer(cx, JS::NewArrayBuffer(cx, uuid?16:chars/2));
    if (!buffer) return false;
    size_t bytes=0; bool shared=false; uint8_t* data=nullptr;
    JS::GetArrayBufferLengthAndData(buffer, &bytes, &shared, &data);
    for (size_t offset=0; offset<bytes; ++offset)
      data[offset] = uint8_t((Hex(digits[offset*2])<<4) | Hex(digits[offset*2+1]));
    value.setObject(*buffer);
    if (!JS_SetElement(cx, entries, index, value)) return false;
    value.setBoolean(uuid);
    if(!JS_SetElement(cx,methods,index,value))return false;
  }
  tape.set(JS_NewObjectWithGivenProto(cx, &kTape, nullptr));
  if (!tape) return false;
  JS::SetReservedSlot(tape, Entries, JS::ObjectValue(*entries));
  JS::SetReservedSlot(tape, Methods, JS::ObjectValue(*methods));
  JS::SetReservedSlot(tape, Cursor, JS::Int32Value(0));
  JS::SetReservedSlot(tape, Mismatches, JS::DoubleValue(0));
  return true;
}
bool AttachRandomReplay(JSContext* cx, JS::HandleObject page, JS::HandleObject tape) {
  JSAutoRealm realm(cx, page);
  JS::RootedObject original(cx), wrapped(cx, tape);
  if (!OriginalCrypto(cx, page, &original) || (wrapped && !JS_WrapObject(cx, &wrapped))) return false;
  JS::SetReservedSlot(original, RandomReplay, wrapped ? JS::ObjectValue(*wrapped) : JS::UndefinedValue());
  return true;
}
bool ReadRandomReplay(JSContext* cx, JS::HandleObject crypto, size_t bytes, bool uuid,
                      JS::MutableHandleObject buffer, bool* configured) {
  JS::RootedObject tape(cx, Tape(crypto));
  *configured = tape != nullptr;
  if (!tape) return true;
  JSAutoRealm realm(cx, tape);
  JS::RootedObject entries(cx, &JS::GetReservedSlot(tape, Entries).toObject());
  JS::RootedObject methods(cx,&JS::GetReservedSlot(tape,Methods).toObject());
  uint32_t length=0, index=uint32_t(JS::GetReservedSlot(tape, Cursor).toInt32());
  JS::RootedValue value(cx);
  auto mismatch=[&] {
    JS::SetReservedSlot(tape,Mismatches,JS::DoubleValue(JS::GetReservedSlot(tape,Mismatches).toNumber()+1));
    return false;
  };
  if (!JS::GetArrayLength(cx, entries, &length)) return false;
  if(index>=length)return mismatch();
  if(!JS_GetElement(cx,methods,index,&value))return false;
  if(value.toBoolean()!=uuid)return mismatch();
  if(!JS_GetElement(cx, entries, index, &value))return false;
  buffer.set(&value.toObject());
  return JS::GetArrayBufferByteLength(buffer) == bytes || mismatch();
}
void CommitRandomReplay(JS::HandleObject crypto) {
  JSObject* tape = Tape(crypto);
  JS::SetReservedSlot(tape, Cursor, JS::Int32Value(JS::GetReservedSlot(tape, Cursor).toInt32()+1));
}
bool RandomReplayStatus(JSContext* cx, JS::HandleObject tape, JS::MutableHandleValue result) {
  JS::RootedObject raw(cx, js::UncheckedUnwrap(tape));
  uint32_t length=0;
  int32_t used=0;
  double mismatches=0;
  {
    JSAutoRealm realm(cx, raw);
    JS::RootedObject entries(cx, &JS::GetReservedSlot(raw, Entries).toObject());
    if (!JS::GetArrayLength(cx, entries, &length)) return false;
    used=JS::GetReservedSlot(raw, Cursor).toInt32();
    mismatches=JS::GetReservedSlot(raw,Mismatches).toNumber();
  }
  JS::RootedObject value(cx, JS_NewPlainObject(cx));
  if (!value || !JS_DefineProperty(cx,value,"expected",length,JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx,value,"used",used,JSPROP_ENUMERATE)||
      !JS_DefineProperty(cx,value,"mismatches",mismatches,JSPROP_ENUMERATE)) return false;
  result.setObject(*value);
  return true;
}
}
