#ifndef RUYIJS_DOCUMENT_EXCEPTION_INTERNAL_H
#define RUYIJS_DOCUMENT_EXCEPTION_INTERNAL_H
#include "document_model/exception.h"
#include "js/CallArgs.h"
#include <string>
#include <string_view>

namespace ruyijs::document_model {
// Existing conversion functions are independent of DocumentModel State.
bool String(JSContext*, JS::HandleValue, std::u16string*, bool null_empty);
bool Value(JSContext*, std::u16string_view, JS::MutableHandleValue);
bool TypeError(JSContext*, const char*);
bool CreateException(JSContext*, JS::HandleObject prototype, std::u16string_view message,
                      std::u16string_view name, JS::MutableHandleValue, const JS::CallArgs* = nullptr);
bool ExceptionConstructor(JSContext*, unsigned, JS::Value*);
bool ExceptionGetter(JSContext*, unsigned, JS::Value*);
}
#endif
