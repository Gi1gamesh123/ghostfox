#ifndef RUYIJS_DOCUMENT_EXCEPTION_H
#define RUYIJS_DOCUMENT_EXCEPTION_H
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
namespace ruyijs::document_model {
// Read only native DOMException storage. Never evaluates a property or proxy trap.
bool ExceptionFields(JS::HandleObject, JS::MutableHandleString, JS::MutableHandleString);
// The sole realm-local installer. It needs no document tree or Window state.
bool InstallExceptionSurface(JSContext*, JS::HandleObject global,
                              JS::MutableHandleObject prototype, JS::MutableHandleObject constructor);
// Native clients keep the installed prototype, never consult a public constructor.
bool ThrowException(JSContext*, JS::HandleObject prototype, const char* name, const char* message);
// A DOMException subclass instance (RTCError, docs/owners/WEB_RTC.md): the same
// native DOMException storage, so the name/message/code members and
// ExceptionFields read it, plus one value the subclass owner keeps and reads
// back. `prototype` is the subclass interface prototype object.
bool CreateDerivedException(JSContext*, JS::HandleObject prototype, JS::HandleString message,
                            const char* name, JS::HandleValue data, JS::MutableHandleValue result);
// The subclass value of a DOMException (undefined for a plain one); false when
// `object` is not a DOMException. Reads native storage only.
bool DerivedExceptionData(JSObject* object, JS::MutableHandleValue data);
}
#endif
