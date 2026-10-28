#ifndef RUYIJS_GARBAGE_COLLECTION_H
#define RUYIJS_GARBAGE_COLLECTION_H
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs {
void CollectGarbage(JSContext* cx, JS::MutableHandleValue result);
}
#endif
