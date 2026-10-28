#ifndef RUYIJS_WEB_CREDENTIALS_OPTIONS_CONVERT_H
#define RUYIJS_WEB_CREDENTIALS_OPTIONS_CONVERT_H
// WebIDL argument conversion for the two parse*FromJSON statics, with the
// donor binding's error messages ("<prefix>Missing required 'x' member of D.",
// "<prefix><where> can't be converted to a dictionary/sequence.").
#include "web_credentials/internal.h"
#include "js/GCVector.h"
#include <initializer_list>
#include <string>

namespace ruyijs::web_credentials {
// One converted sequence<PublicKeyCredentialDescriptorJSON>, before decoding.
struct DescriptorList {
  explicit DescriptorList(JSContext* cx) : ids(cx), transports(cx), types(cx) {}
  JS::RootedValueVector ids, transports, types;
};

class Converter {
 public:
  Converter(State& owner, const char16_t* label) : state(owner), cx(owner.cx), prefix(label) {}
  State& state;
  JSContext* cx;
  std::u16string prefix;

  bool MissingArgument();
  bool Dictionary(JS::HandleValue value, const std::u16string& where, JS::MutableHandleObject out);
  bool Member(JS::HandleObject dictionary, const char* name, JS::MutableHandleValue out);
  bool Required(JS::HandleValue value, const char16_t* member, const char16_t* dictionary);
  bool Text(JS::MutableHandleValue value);
  bool Text(JS::HandleValue value, std::u16string* out);
  bool Sequence(JS::HandleValue value, const std::u16string& where, JS::MutableHandleValueVector items);
  bool TextSequence(JS::MutableHandleValue value, const std::u16string& where);
  bool UnsignedLong(JS::MutableHandleValue value);
  bool Enumeration(JS::HandleValue value, const char16_t* member, const char16_t* dictionary,
                   const char16_t* enumeration, std::initializer_list<const char16_t*> values);
  bool Descriptors(JS::HandleValue value, const char16_t* member, const char16_t* owner,
                   DescriptorList* list);
  bool DecodeDescriptors(const DescriptorList& list, const char16_t* what, JS::MutableHandleValue out);
  bool Decoded(JS::HandleValue text, const char16_t* what, JS::MutableHandleValue out);
  bool Extensions(JS::HandleValue value, const char16_t* owner, bool creation, JS::MutableHandleValue out);
  bool Selection(JS::HandleValue value, const char16_t* owner, JS::MutableHandleValue out);
  bool Parameters(JS::HandleValue value, const char16_t* owner, JS::MutableHandleValue out);
  bool RelyingParty(JS::HandleValue value, const char16_t* owner, JS::MutableHandleValue out);
  bool User(JS::HandleValue value, const char16_t* owner, JS::MutableHandleValue display,
            JS::MutableHandleValue id, JS::MutableHandleValue name);

  JSObject* Array();
  bool Push(JS::HandleObject array, JS::HandleValue value);
  bool Set(JS::HandleObject object, const char* name, JS::HandleValue value);
  bool Literal(const char16_t* text, JS::MutableHandleValue out);
  bool Buffer(const std::string& bytes, JS::MutableHandleValue out);
};
}
#endif
