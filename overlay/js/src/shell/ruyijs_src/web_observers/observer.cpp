// MutationObserver members and observer bookkeeping: option parsing, the
// replace-or-add registration list and the per-observer record queue. The
// records themselves come from the Document Model's real-change log; this file
// owns every JavaScript-visible policy decision.
#include "runtime/web_kit.h"
#include "web_observers/internal.h"
#include "web_observers/realm.h"
#include "document_model/realm.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/StableStringChars.h"
#include "js/String.h"
#include "js/Wrapper.h"
#include "jsfriendapi.h"
#include <cmath>
#include <new>
#include <utility>

namespace ruyijs::web_observers {
namespace {
// A dictionary boolean member: absent stays absent so the two implied flags of
// observe() below can tell "not passed" from "passed false".
bool Option(JSContext* cx, JS::HandleObject options, const char* name,
            bool* present, bool* value) {
  JS::RootedValue member(cx);
  if (!JS_GetProperty(cx, options, name, &member)) return false;
  *present = !member.isUndefined();
  *value = *present && JS::ToBoolean(member);
  return true;
}

// attributeFilter: sequence<DOMString>, read in bounds. A non-object (null
// included) is a TypeError, like the WebIDL sequence conversion.
bool Filters(JSContext* cx, JS::HandleObject options, State& state,
             bool* present, std::vector<DomString>* out) {
  JS::RootedValue member(cx);
  if (!JS_GetProperty(cx, options, "attributeFilter", &member)) return false;
  *present = !member.isUndefined();
  if (!*present) return true;
  if (!member.isObject())
    return state.document->ThrowTypeError("attributeFilter must be a sequence of strings");
  JS::RootedObject sequence(cx, &member.toObject());
  JS::RootedValue length(cx);
  if (!JS_GetProperty(cx, sequence, "length", &length)) return false;
  double count = 0;
  if (!JS::ToNumber(cx, length, &count)) return false;
  if (!std::isfinite(count) || count < 0) count = 0;
  if (count > 4096)
    return state.document->ThrowTypeError("attributeFilter exceeds the product limit");
  for (uint32_t index = 0; index < uint32_t(count); ++index) {
    JS::RootedValue item(cx);
    if (!JS_GetElement(cx, sequence, index, &item)) return false;
    JS::RootedString text(cx, JS::ToString(cx, item));
    if (!text) return false;
    JS::AutoStableStringChars chars(cx);
    if (!chars.initTwoByte(cx, text)) return false;
    out->emplace_back(chars.twoByteChars(), chars.length());
  }
  return true;
}

// The validation order and the two implied flags are the donor's
// nsDOMMutationObserver::Observe, not a re-invention.
bool Parse(JSContext* cx, JS::HandleObject options, State& state, Registration* registration) {
  bool present = false;
  bool child_list = false, attributes = false, character_data = false, subtree = false;
  bool attribute_old_value = false, character_data_old_value = false;
  bool attributes_present = false, character_present = false;
  bool attribute_old_present = false, character_old_present = false;
  bool filter_present = false;
  std::vector<DomString> filter;
  if (options &&
      (!Option(cx, options, "childList", &present, &child_list) ||
       !Option(cx, options, "attributes", &attributes_present, &attributes) ||
       !Option(cx, options, "characterData", &character_present, &character_data) ||
       !Option(cx, options, "subtree", &present, &subtree) ||
       !Option(cx, options, "attributeOldValue", &attribute_old_present, &attribute_old_value) ||
       !Option(cx, options, "characterDataOldValue", &character_old_present,
               &character_data_old_value) ||
       !Filters(cx, options, state, &filter_present, &filter))) return false;
  if (!attributes_present) attributes = attribute_old_present || filter_present;
  if (!character_present) character_data = character_old_present;
  if (!(child_list || attributes || character_data))
    return state.document->ThrowTypeError(
        "One of 'childList', 'attributes', 'characterData' must not be false.");
  if (attribute_old_present && attribute_old_value && !attributes)
    return state.document->ThrowTypeError(
        "If 'attributeOldValue' is true, 'attributes' must not be false.");
  if (filter_present && !attributes)
    return state.document->ThrowTypeError(
        "If 'attributeFilter' is present, 'attributes' must not be false.");
  if (character_old_present && character_data_old_value && !character_data)
    return state.document->ThrowTypeError(
        "If 'characterDataOldValue' is true, 'characterData' must not be false.");
  registration->child_list = child_list;
  registration->attributes = attributes;
  registration->character_data = character_data;
  registration->subtree = subtree;
  registration->attribute_old_value = attribute_old_value;
  registration->character_data_old_value = character_data_old_value;
  registration->all_attributes = !filter_present;
  registration->filter = std::move(filter);
  return true;
}

// The branded receiver, or a TypeError for any other this value.
Observer* Receiver(JSContext* cx, const JS::CallArgs& args) {
  JSObject* object = args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr;
  if (object && JS::GetClass(object) == &kObserverClass) {
    auto* observer =
        static_cast<Observer*>(JS::GetReservedSlot(object, 0).toPrivate());
    if (observer) return observer;
  }
  WebKit::FailBrand(cx, args, "MutationObserver");
  return nullptr;
}
}  // namespace

State::State(JSContext* context, JS::HandleObject global, PageHelpers& trace,
             document_model::Realm* owner)
    : cx(context), page(context, global), observerPrototype(context),
      recordPrototype(context), deliver(context), elementPrototype(context),
      documentPrototype(context), intersectionPrototype(context),
      intersectionEntryPrototype(context), resizePrototype(context),
      resizeEntryPrototype(context), resizeSizePrototype(context),
      reportingPrototype(context), reportPrototype(context), reportBodyPrototype(context),
      intersectionDeliver(context), resizeDeliver(context), helpers(&trace),
      document(owner), mutationGroup(std::make_shared<MutationGroup>()) {
  mutationGroup->sources.push_back(this);
}

Observer* State::New(JS::HandleObject object, JS::HandleObject callback) {
  if (count >= kObserverLimit) {
    document->ThrowTypeError("MutationObserver limit exceeded");
    return nullptr;
  }
  std::unique_ptr<Observer> created(new (std::nothrow) Observer(cx, object, callback));
  if (!created) { JS_ReportOutOfMemory(cx); return nullptr; }
  created->owner = this;
  ++count;
  // Delivery runs in creation order, matching the donor's ascending mId.
  Observer* added = created.get();
  mutationGroup->Link(*added);
  if (!observers) {
    observers = std::move(created);
  } else {
    Observer* tail = observers.get();
    while (tail->next) tail = tail->next.get();
    tail->next = std::move(created);
  }
  return added;
}

bool ObserverConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  if (!state) { JS_ReportErrorASCII(cx, "Illegal constructor."); return false; }
  if (!args.isConstructing()) return state->document->ThrowTypeError("Illegal constructor.");
  if (!args.get(0).isObject() || !JS::IsCallable(&args.get(0).toObject()))
    return state->document->ThrowTypeError(
        "MutationObserver requires a callable mutation callback");
  JS::RootedObject callback(cx, &args.get(0).toObject());
  JS::RootedObject object(
      cx, JS_NewObjectWithGivenProto(cx, &kObserverClass, state->observerPrototype));
  if (!object) return false;
  Observer* observer = state->New(object, callback);
  if (!observer) return false;
  JS::SetReservedSlot(object, 0, JS::PrivateValue(observer));
  args.rval().setObject(*object);
  return state->helpers->Object("MutationObserver", object) && trace.Finish(true);
}

bool RecordConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  if (!state) { JS_ReportErrorASCII(cx, "Illegal constructor."); return false; }
  return state->document->ThrowTypeError("Illegal constructor.");
}

bool RecordMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JSObject* object = args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr;
  if (!object || JS::GetClass(object) != &kRecordClass) {
    auto* state = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
    if (state)
      return state->document->ThrowTypeError(
          "MutationRecord member called on an incompatible receiver");
    JS_ReportErrorASCII(cx, "MutationRecord member called on an incompatible receiver");
    return false;
  }
  int32_t slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  args.rval().set(JS::GetReservedSlot(object, size_t(slot)));
  return JS_WrapValue(cx, args.rval());
}

bool Observe(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Observer* observer = Receiver(cx, args);
  if (!observer) return false;
  State& state = *observer->owner;
  auto& callee = *static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  document_model::NodeSource target;
  if (!callee.document->ResolveNodeSource(args.get(0), &target)) return false;
  if (!args.get(1).isUndefined() && !args.get(1).isObject())
    return callee.document->ThrowTypeError("MutationObserver.observe requires an options object");
  JS::RootedObject options(cx);
  if (args.get(1).isObject()) options = &args.get(1).toObject();
  Registration registration;
  if (!Parse(cx, options, callee, &registration)) return false;
  // Dictionary getters may adopt the original target. Brand validation above
  // still precedes conversion, but registration uses the committed identity.
  if (!callee.document->ResolveNodeSource(args.get(0), &target)) return false;
  registration.source = state.mutationGroup->Find(target.owner);
  if (!registration.source)
    return callee.document->ThrowTypeError("MutationObserver target belongs to another Context");
  registration.target = target.key;
  registration.baseline = target.owner->MutationSequence();
  state.Observe(*observer, registration);
  args.rval().setUndefined();
  return true;
}

bool Disconnect(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Observer* observer = Receiver(cx, args);
  if (!observer) return false;
  observer->owner->Disconnect(*observer);
  args.rval().setUndefined();
  return true;
}

bool TakeRecords(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Observer* observer = Receiver(cx, args);
  if (!observer) return false;
  JS::RootedObject records(cx);
  if (!observer->owner->TakeRecords(*observer, &records)) return false;
  args.rval().setObject(*records);
  return true;
}
}  // namespace ruyijs::web_observers



