#ifndef RUYIJS_TYPED_DECODE_H
#define RUYIJS_TYPED_DECODE_H
#include <cstddef>
#include <string>
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs {
bool DecodeTyped(JSContext* cx, JS::HandleValue encoded,
                  JS::MutableHandleValue decoded);
class TypedDecoder {
 public:
  explicit TypedDecoder(JSContext* cx) : cx_(cx) {}
  bool Decode(JS::HandleValue encoded, JS::MutableHandleValue decoded,
               size_t depth = 0);
 private:
  JSContext* cx_;
  size_t nodes_ = 0;
  size_t textBytes_ = 0;
  bool Text(JS::HandleValue value, std::string* text);
  bool Container(JS::HandleValue input, bool array, size_t depth,
                  JS::MutableHandleValue decoded);
};
}
#endif
