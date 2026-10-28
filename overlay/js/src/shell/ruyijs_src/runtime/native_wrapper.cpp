#include "runtime/host_bridge.h"
#include "runtime/page_helpers.h"
#include "runtime/native_constructor_trace.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/CompilationAndEvaluation.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/SourceText.h"
#include "js/StableStringChars.h"
#include "js/ValueArray.h"

namespace ruyijs {
namespace {
bool ConstructorCall(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  JS::RootedValue delegate(cx, js::GetFunctionNativeReserved(&args.callee(), 0));
  if (!args.isConstructing())
    return trace.Finish(JS::Call(cx, args.thisv(), delegate,
                                JS::HandleValueArray(args), args.rval()));
  JS::RootedObject target(cx, &args.newTarget().toObject()), instance(cx);
  if (!JS::Construct(cx, delegate, target, JS::HandleValueArray(args), &instance))
    return trace.Finish(false);
  args.rval().setObject(*instance);
  return trace.Finish(true);
}

bool ConstructorWrapper(JSContext* cx, JS::MutableHandleValue wrapper) {
  JS::RootedObject delegate(cx, &wrapper.toObject());
  JS::RootedValue length(cx);
  if (!JS_GetProperty(cx, delegate, "length", &length)) return false;
  JS::RootedFunction function(cx, js::NewFunctionWithReserved(
      cx, ConstructorCall, length.toInt32(), JSFUN_CONSTRUCTOR, nullptr));
  if (!function) return false;
  JS::RootedObject object(cx, JS_GetFunctionObject(function));
  js::SetFunctionNativeReserved(object, 0, wrapper);
  JS::RootedObject prototype(cx, JS_NewPlainObject(cx));
  if (!prototype || !JS_DefineProperty(cx, prototype, "constructor", object, 0) ||
      !JS_DefineProperty(cx, object, "prototype", prototype, JSPROP_PERMANENT))
    return false;
  wrapper.setObject(*object);
  return true;
}
}

bool HostBridge::NativeWrap(JS::HandleString name, JS::HandleString source, bool constructor) {
  JS::AutoStableStringChars characters(cx_);
  if (!characters.initTwoByte(cx_, source)) return false;
  std::u16string text(u"(");
  text.append(characters.twoByteChars(), characters.length());
  text.append(u"\n)");
  JSAutoRealm realm(cx_, page_);
  JS::RootedId id(cx_);
  JS::RootedString localName(cx_);
  if (!Name(name, &id, &localName)) return false;
  JS::CompileOptions options(cx_);
  options.setFileAndLine("ruyijs:native", 1);
  JS::SourceText<char16_t> code;
  if (!code.init(cx_, text.data(), text.size(), JS::SourceOwnership::Borrowed))
    return false;
  JS::RootedValue implementation(cx_);
  if (!JS::Evaluate(cx_, options, code, &implementation)) return false;
  if (!implementation.isObject() || !JS::IsCallable(&implementation.toObject())) {
    JS_ReportErrorASCII(cx_, "Native wrapper source must evaluate to a function");
    return false;
  }
  JS::RootedObject function(cx_, &implementation.toObject());
  JS::RootedValueArray<4> arguments(cx_);
  arguments[0].set(implementation);
  arguments[1].setString(localName);
  if (!JS_GetProperty(cx_, function, "length", arguments[2])) return false;
  arguments[3].setBoolean(constructor);
  JS::RootedValue wrapper(cx_);
  if (!helpers_->Call("createHostFunction", arguments, &wrapper)) return false;
  if (constructor && !ConstructorWrapper(cx_, &wrapper)) return false;
  JS::RootedObject native(cx_, &wrapper.toObject());
  if (!JS_DefineProperty(cx_, native, "name", localName, JSPROP_READONLY)) return false;
  return RegisterFunction(native, localName) &&
         JS_DefinePropertyById(cx_, page_, id, wrapper, JSPROP_ENUMERATE) &&
         Record(Kind::Native, name, source, constructor, JS::UndefinedHandleValue);
}
}
