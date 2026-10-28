#include "web_graphics/internal.h"
#include <cstring>
#include "js/Symbol.h"
#include "js/CharacterEncoding.h"
#include "runtime/page_helpers.h"
namespace ruyijs::web_graphics {
namespace {
const JSClass kExtensionClass = {"WebGLExtension", JSCLASS_HAS_RESERVED_SLOTS(2)};
struct Extension { const char* name; const char* tag; };
const Extension kExtensions[] = {
#include "web_graphics/extension_defs_0.inc"
};
struct Constant { int extension; const char* name; double value; };
const Constant kConstants[] = {
#include "web_graphics/extension_constants_0.inc"
};
struct Method { int extension; const char* name; unsigned length; int op; };
const Method kMethods[] = {
#include "web_graphics/extension_methods_0.inc"
};
bool ExtensionCall(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *detail::Self(args);
  int slot = WebKit::Slot(args), op = slot % 64;
  JS::RootedObject self(cx);
  if (!WebKit::Check(cx, args.thisv(), &kExtensionClass, &self, "Illegal invocation")) return false;
  if (JS::GetReservedSlot(self, 1).toInt32() != slot/64) return WebKit::Fail(cx, "Illegal invocation");
  auto* context = static_cast<Context*>(JS::GetReservedSlot(self, 0).toPrivate());
  args.rval().setUndefined();
  if (op >= 1 && op <= 6) {
    int kind = op % 2 ? kQueryObject : kVertexArrayObject;
    if (op <= 2) {
      JS::RootedObject object(cx);
      if (!NewObject(s, kind, *context, &object)) return false;
      args.rval().setObject(*object); return true;
    }
    JS::HandleValue v = args.get(0);
    bool owned = v.isObject() && JS::GetClass(&v.toObject()) == s.Face(kind).clasp &&
        detail::ObjectContext(&v.toObject()) == context;
    if (op <= 4) args.rval().setBoolean(owned && !detail::ObjectDeleted(&v.toObject()));
    else if (owned) JS::SetReservedSlot(&v.toObject(), detail::kObjectDeletedSlot, JS::BooleanValue(true));
    else if (!v.isNullOrUndefined()) detail::SetError(*context, kInvalidOperation);
    return true;
  }
  if (op == 7) {
    JS::RootedString text(cx, JS_NewStringCopyZ(cx, ""));
    if (!text) return false;
    args.rval().setString(text);
  } else if (op == 8) {
    JS::RootedObject profiles(cx, JS::NewArrayObject(cx, 1));
    JS::RootedString name(cx, JS_NewStringCopyZ(cx, "ldr"));
    if (!profiles || !name || !JS_SetElement(cx, profiles, 0, name)) return false;
    args.rval().setObject(*profiles);
  } else if (op == 9) args.rval().setNull();
  return true;
}
}
bool NewExtension(State& s, Context& context, JS::HandleString name, JS::MutableHandleValue out) {
  JS::UniqueChars bytes(JS_EncodeStringToUTF8(s.cx, name));
  if (!bytes) return false;
  const auto& values = s.model.values();
  const auto& supported = context.version == 2 ? values.supported_extensions2 : values.supported_extensions;
  const char* canonical = nullptr;
  for (const auto& candidate : supported) {
    if (JS_GetStringLength(name) != candidate.size()) continue;
    bool match = true;
    for (size_t i = 0; i < candidate.size(); ++i) {
      auto lower = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a'-'A') : c; };
      if (lower(bytes[i]) != lower(candidate[i])) { match = false; break; }
    }
    if (match) { canonical = candidate.c_str(); break; }
  }
  if (!canonical) { out.setNull(); return true; }
  for (size_t i = 0; i < std::size(kExtensions); ++i) {
    if (std::string(canonical) != kExtensions[i].name) continue;
    if (!context.extensions) context.extensions = JS::NewArrayObject(s.cx, std::size(kExtensions));
    if (!context.extensions) return false;
    JS::RootedValue cached(s.cx);
    if (!JS_GetElement(s.cx, context.extensions, i, &cached)) return false;
    if (cached.isObject()) { out.set(cached); return true; }
    JS::RootedValue proto(s.cx);
    if (!JS_GetElement(s.cx, s.extensionPrototypes, i, &proto)) return false;
    JS::RootedObject prototype(s.cx, &proto.toObject());
    JS::RootedObject object(s.cx, s.kit.New(&kExtensionClass, prototype));
    if (!object) return false;
    JS::SetReservedSlot(object, 0, JS::PrivateValue(&context));
    JS::SetReservedSlot(object, 1, JS::Int32Value(i));
    if (!JS_SetElement(s.cx, context.extensions, i, object) ||
        !s.kit.helpers->Object(kExtensions[i].tag, object)) return false;
    out.setObject(*object); return true;
  }
  out.setNull(); return true;
}
bool ExtensionEnabled(State& s, Context& context, const char* name, bool* enabled) {
  *enabled = false;
  if (!context.extensions) return true;
  for (size_t i = 0; i < std::size(kExtensions); ++i) {
    if (std::strcmp(name, kExtensions[i].name) != 0) continue;
    JS::RootedValue cached(s.cx);
    if (!JS_GetElement(s.cx, context.extensions, i, &cached)) return false;
    *enabled = cached.isObject();
    return true;
  }
  return true;
}
bool InstallExtensions(State& s) {
  s.extensionPrototypes = JS::NewArrayObject(s.cx, std::size(kExtensions));
  if (!s.extensionPrototypes) return false;
  JS::RootedId tagId(s.cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(s.cx, JS::SymbolCode::toStringTag)));
  for (size_t i = 0; i < std::size(kExtensions); ++i) {
    WebFace face(s.cx);
    face.prototype = JS_NewPlainObject(s.cx);
    JS::RootedString tag(s.cx, JS_NewStringCopyZ(s.cx, kExtensions[i].tag));
    if (!face.prototype || !tag ||
        !JS_DefinePropertyById(s.cx, face.prototype, tagId, tag, JSPROP_READONLY) ||
        !JS_SetElement(s.cx, s.extensionPrototypes, i, face.prototype)) return false;
    for (const auto& c : kConstants) if (c.extension == int(i)) {
      JS::RootedValue value(s.cx, JS::NumberValue(c.value));
      if (!JS_DefineProperty(s.cx, face.prototype, c.name, value,
          JSPROP_READONLY|JSPROP_PERMANENT|JSPROP_ENUMERATE)) return false;
    }
    for (const auto& m : kMethods) if (m.extension == int(i))
      if (!s.kit.Method(face, kExtensions[i].tag, m.name, ExtensionCall, m.length, m.op)) return false;
    if (!s.kit.helpers->Interface(kExtensions[i].tag, face.prototype)) return false;
  }
  return true;
}
}
