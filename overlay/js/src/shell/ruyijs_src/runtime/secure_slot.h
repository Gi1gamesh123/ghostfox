#ifndef RUYIJS_RUNTIME_SECURE_SLOT_H
#define RUYIJS_RUNTIME_SECURE_SLOT_H
#include <memory>
#include <string>
#include <vector>
#include "jsapi.h"
#include "js/PropertyAndElement.h"
#include "js/PropertyDescriptor.h"

namespace ruyijs {
// One [SecureContext] own property of an owner's object (an interface object on
// the global or a member on a prototype). The owner installs it normally, then
// calls Apply after every secure-context change: an insecure realm loses the
// property, a secure one gets the same descriptor back. The descriptor is kept
// here, so no second installer exists. Owner-held; one per property.
class SecureSlot final {
 public:
  SecureSlot(JSContext* cx, const char* name) : name_(name), target_(cx), saved_(cx) {}
  bool Apply(JSContext* cx, JS::HandleObject target, bool secure) {
    JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> current(cx);
    if (!JS_GetOwnPropertyDescriptor(cx, target, name_.c_str(), &current)) return false;
    if (current.isSome()) {
      target_ = target;
      saved_.set(*current);
      has_ = true;
    }
    if (!secure) return current.isNothing() || JS_DeleteProperty(cx, target, name_.c_str());
    if (current.isSome() || !has_ || target_ != target) return true;
    JS::Rooted<JS::PropertyDescriptor> desc(cx, saved_.get());
    JS::RootedString atom(cx, JS_AtomizeString(cx, name_.c_str()));
    JS::RootedId id(cx);
    return atom && JS_StringToId(cx, atom, &id) && JS_DefinePropertyById(cx, target, id, desc);
  }
 private:
  std::string name_;
  JS::PersistentRootedObject target_;
  JS::PersistentRooted<JS::PropertyDescriptor> saved_;
  bool has_ = false;
};
// A list of an owner's [SecureContext] properties on several targets.
class SecureSlots final {
 public:
  explicit SecureSlots(JSContext* cx) : cx_(cx) {}
  bool Add(JS::HandleObject target, const char* name) {
    entries_.push_back({std::make_unique<SecureSlot>(cx_, name), JS::PersistentRootedObject(cx_, target)});
    return true;
  }
  bool Apply(bool secure) {
    for (auto& entry : entries_) {
      JS::RootedObject target(cx_, entry.target);
      if (!entry.slot->Apply(cx_, target, secure)) return false;
    }
    return true;
  }
 private:
  struct Entry { std::unique_ptr<SecureSlot> slot; JS::PersistentRootedObject target; };
  JSContext* cx_;
  std::vector<Entry> entries_;
};
}  // namespace ruyijs
#endif
