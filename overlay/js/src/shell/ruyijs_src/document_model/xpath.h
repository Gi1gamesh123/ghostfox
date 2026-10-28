#pragma once
#include "js/CallArgs.h"
#include "js/Class.h"
#include "js/RootingAPI.h"

namespace ruyijs::document_model {
struct State;
enum class XPathKind { Evaluator, Expression, Result };
extern const JSClass kXPathClass;
bool NewXPath(State&, XPathKind, uint16_t, JS::MutableHandleValue,
              const JS::CallArgs* = nullptr);
// Returns a checked-unwrapped object rooted by the caller.
bool XPathReceiver(JSContext*, JS::HandleValue, XPathKind, JS::MutableHandleObject);
bool InstallXPath(State&);
bool InstallXPathResult(State&);
}
