// WebIDL interface object inheritance, applied once after a realm's last
// installer ran. Each owner creates its interface object and the prototype chain of its
// interface prototype object; WebIDL derives the interface object's own
// [[Prototype]] from that chain ("create an interface object", step 3): the
// interface object of the inherited interface, or %Function.prototype% for an
// interface with none. The donor's generated bindings pass the parent's
// interface object to CreateInterfaceObjects; this pass gives every owner the
// same result without a second rule per installer. It also puts each interface
// prototype object's existing "constructor" property last, where the binding
// defines it; it creates no property.
#include "runtime/interface_objects.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/PropertyAndElement.h"
#include "js/PropertyDescriptor.h"
#include "js/Realm.h"

namespace ruyijs {
namespace {
// The object's own data property |name| when it is an object, else null.
JSObject* OwnObject(JSContext* cx, JS::HandleObject target, const char* name, bool* ok) {
  JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> descriptor(cx);
  *ok = JS_GetOwnPropertyDescriptor(cx, target, name, &descriptor);
  if (!*ok || descriptor.isNothing() || !descriptor->isDataDescriptor() ||
      !descriptor->value().isObject()) return nullptr;
  return &descriptor->value().toObject();
}
// An interface object: a function whose own prototype object names it back as
// its constructor. Some owners create theirs without [[Construct]]; that does
// not make it another kind of object. A legacy factory function (Image, Audio,
// Option) shares another interface's prototype and so is not one.
bool InterfacePrototype(JSContext* cx, JS::HandleObject function, JS::MutableHandleObject prototype,
                        bool* ok) {
  prototype.set(nullptr);
  *ok = true;
  if (!JS_ObjectIsFunction(function) || JS::IdentifyStandardConstructor(function) != JSProto_Null)
    return false;
  JS::RootedObject candidate(cx, OwnObject(cx, function, "prototype", ok));
  if (!*ok || !candidate) return false;
  JS::RootedObject back(cx, OwnObject(cx, candidate, "constructor", ok));
  if (!*ok || back != function) return false;
  prototype.set(candidate);
  return true;
}
// The binding defines an interface prototype object's members first and its
// "constructor" property when it links the interface object, so "constructor"
// is the last own key (Object.getOwnPropertyNames order). Owners here create
// the pair before their members; the property is redefined, unchanged, last.
bool ConstructorLast(JSContext* cx, JS::HandleObject prototype) {
  JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> descriptor(cx);
  if (!JS_GetOwnPropertyDescriptor(cx, prototype, "constructor", &descriptor)) return false;
  if (descriptor.isNothing() || !descriptor->configurable()) return true;
  JS::RootedValue value(cx, descriptor->value());
  unsigned attrs = (descriptor->enumerable() ? JSPROP_ENUMERATE : 0) |
                   (descriptor->writable() ? 0 : JSPROP_READONLY);
  return descriptor->isDataDescriptor() && JS_DeleteProperty(cx, prototype, "constructor") &&
         JS_DefineProperty(cx, prototype, "constructor", value, attrs);
}
}  // namespace

bool LinkInterfaceObjects(JSContext* cx, JS::HandleObject global) {
  JSAutoRealm realm(cx, global);
  JS::RootedObject functionPrototype(cx, JS::GetRealmFunctionPrototype(cx));
  JS::RootedIdVector ids(cx);
  if (!functionPrototype || !js::GetPropertyKeys(cx, global, JSITER_OWNONLY | JSITER_HIDDEN, &ids))
    return false;
  for (size_t i = 0; i < ids.length(); ++i) {
    if (!ids[i].isString()) continue;
    JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> descriptor(cx);
    if (!JS_GetOwnPropertyDescriptorById(cx, global, ids[i], &descriptor)) return false;
    if (descriptor.isNothing() || !descriptor->isDataDescriptor() || !descriptor->value().isObject())
      continue;
    JS::RootedObject function(cx, &descriptor->value().toObject()), prototype(cx);
    bool ok = true;
    if (!InterfacePrototype(cx, function, &prototype, &ok)) {
      if (!ok) return false;
      continue;
    }
    // The nearest inherited interface: walk the prototype chain to the first
    // object that is some interface object's prototype.
    JS::RootedObject parent(cx), cursor(cx), owner(cx), back(cx);
    if (!JS_GetPrototype(cx, prototype, &cursor)) return false;
    while (cursor) {
      owner = OwnObject(cx, cursor, "constructor", &ok);
      if (!ok) return false;
      if (owner && InterfacePrototype(cx, owner, &back, &ok) && back == cursor) {
        parent = owner;
        break;
      }
      if (!ok) return false;
      // An ECMAScript constructor (Object, Error) is not an inherited interface.
      if (owner && JS::IdentifyStandardConstructor(owner) != JSProto_Null) break;
      if (!JS_GetPrototype(cx, cursor, &cursor)) return false;
    }
    JS::RootedObject expected(cx, parent ? parent.get() : functionPrototype.get()), current(cx);
    if (!JS_GetPrototype(cx, function, &current)) return false;
    if (current != expected && !JS_SetPrototype(cx, function, expected)) return false;
    if (!ConstructorLast(cx, prototype)) return false;
  }
  return true;
}
}  // namespace ruyijs
