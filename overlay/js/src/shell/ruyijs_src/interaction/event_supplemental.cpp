// The T3c page and UI payload events: PageTransitionEvent, PromiseRejectionEvent,
// SubmitEvent, ClipboardEvent, TransitionEvent, AnimationEvent,
// SecurityPolicyViolationEvent, MediaQueryListEvent and BeforeUnloadEvent. All
// nine are direct Event subclasses, so this TU owns their property tables and
// their dictionary adapter and nothing else; the MouseEvent/UIEvent-derived
// classes live in event_input_extra.cpp, the device sensor classes in
// event_device.cpp and the WebRTC payload classes in event_rtc.cpp. The split
// keeps every table TU on the same budget event_navigation.cpp and
// event_offline_audio.cpp already follow.
#include "interaction/internal.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include <string>

namespace ruyijs::interaction {
namespace {
JSObject* Self(const JS::CallArgs& args) { return &args.thisv().toObject(); }
// Read-only member of one of the nine interfaces: brand-check the receiver,
// report the access of the installed member and re-wrap the traced slot the
// constructor wrote.
bool Slot(JSContext* cx, const JS::CallArgs& args, const char* member, Iface iface,
          uint32_t slot) {
  Event* event = Receiver(cx, args, iface);
  if (!event) return false;
  ReportEventAccess(cx, event, InterfaceName(iface), member, int(ApiAccess::Read));
  args.rval().set(JS::GetReservedSlot(Self(args), slot));
  return JS_WrapValue(cx, args.rval());
}
#define EVENT_MEMBER_GETTER(name, member, iface, slot)                  \
  bool name(JSContext* cx, unsigned argc, JS::Value* vp) {              \
    return Slot(cx, JS::CallArgsFromVp(argc, vp), member, iface, slot); \
  }

// PageTransitionEvent
EVENT_MEMBER_GETTER(GetPagePersisted, "persisted", Iface::PageTransition,
                    kEventPagePersisted)
// PromiseRejectionEvent: the interface exposes the promise under its WebIDL
// name, the internal member is `rejectedPromise` (BinaryName).
EVENT_MEMBER_GETTER(GetRejectedPromise, "promise", Iface::PromiseRejection,
                    kEventPromise)
EVENT_MEMBER_GETTER(GetRejectionReason, "reason", Iface::PromiseRejection,
                    kEventRejectionReason)
// SubmitEvent
EVENT_MEMBER_GETTER(GetSubmitter, "submitter", Iface::Submit, kEventSubmitter)
// TransitionEvent
EVENT_MEMBER_GETTER(GetTransitionProperty, "propertyName", Iface::Transition,
                    kEventTransitionProperty)
EVENT_MEMBER_GETTER(GetTransitionElapsed, "elapsedTime", Iface::Transition,
                    kEventTransitionElapsed)
EVENT_MEMBER_GETTER(GetTransitionPseudo, "pseudoElement", Iface::Transition,
                    kEventTransitionPseudo)
EVENT_MEMBER_GETTER(GetTransitionAnimation, "animation", Iface::Transition,
                    kEventTransitionAnimation)
// AnimationEvent
EVENT_MEMBER_GETTER(GetAnimationName, "animationName", Iface::Animation,
                    kEventAnimationName)
EVENT_MEMBER_GETTER(GetAnimationElapsed, "elapsedTime", Iface::Animation,
                    kEventAnimationElapsed)
EVENT_MEMBER_GETTER(GetAnimationPseudo, "pseudoElement", Iface::Animation,
                    kEventAnimationPseudo)
EVENT_MEMBER_GETTER(GetAnimationObject, "animation", Iface::Animation,
                    kEventAnimationObject)
// SecurityPolicyViolationEvent
EVENT_MEMBER_GETTER(GetPolicyDocumentUri, "documentURI", Iface::PolicyViolation,
                    kEventPolicyDocumentUri)
EVENT_MEMBER_GETTER(GetPolicyReferrer, "referrer", Iface::PolicyViolation,
                    kEventPolicyReferrer)
EVENT_MEMBER_GETTER(GetPolicyBlockedUri, "blockedURI", Iface::PolicyViolation,
                    kEventPolicyBlockedUri)
EVENT_MEMBER_GETTER(GetPolicyViolatedDirective, "violatedDirective",
                    Iface::PolicyViolation, kEventPolicyViolatedDirective)
EVENT_MEMBER_GETTER(GetPolicyEffectiveDirective, "effectiveDirective",
                    Iface::PolicyViolation, kEventPolicyEffectiveDirective)
EVENT_MEMBER_GETTER(GetPolicyOriginalPolicy, "originalPolicy",
                    Iface::PolicyViolation, kEventPolicyOriginalPolicy)
EVENT_MEMBER_GETTER(GetPolicySourceFile, "sourceFile", Iface::PolicyViolation,
                    kEventPolicySourceFile)
EVENT_MEMBER_GETTER(GetPolicySample, "sample", Iface::PolicyViolation,
                    kEventPolicySample)
EVENT_MEMBER_GETTER(GetPolicyDisposition, "disposition", Iface::PolicyViolation,
                    kEventPolicyDisposition)
EVENT_MEMBER_GETTER(GetPolicyStatusCode, "statusCode", Iface::PolicyViolation,
                    kEventPolicyStatusCode)
EVENT_MEMBER_GETTER(GetPolicyLineNumber, "lineNumber", Iface::PolicyViolation,
                    kEventPolicyLineNumber)
EVENT_MEMBER_GETTER(GetPolicyColumnNumber, "columnNumber", Iface::PolicyViolation,
                    kEventPolicyColumnNumber)
// MediaQueryListEvent
EVENT_MEMBER_GETTER(GetMediaQueryMedia, "media", Iface::MediaQueryList,
                    kEventMediaQueryMedia)
EVENT_MEMBER_GETTER(GetMediaQueryMatches, "matches", Iface::MediaQueryList,
                    kEventMediaQueryMatches)
#undef EVENT_MEMBER_GETTER

// ClipboardEvent.clipboardData: the donor creates a DataTransfer holding the
// dictionary's data/dataType pair (dom/events/ClipboardEvent.cpp:41-56). This
// runtime has no DataTransfer owner yet, so the attribute reports null and the
// two converted strings are dropped; the dictionary conversion itself still
// runs, so a throwing data/dataType fails the construction like the donor.
bool GetClipboardData(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::Clipboard);
  if (!event) return false;
  ReportEventAccess(cx, event, "ClipboardEvent", "clipboardData", int(ApiAccess::Read));
  args.rval().setNull();
  return true;
}

// BeforeUnloadEvent is the one T3c interface with a writable attribute: the
// getter reads the stored text and the setter replaces it with String(value)
// (dom/events/BeforeUnloadEvent.cpp GetReturnValue/SetReturnValue). The
// prototype accessor shadows Event.prototype.returnValue, whose boolean
// cancel-bubble form stays on the base interface.
bool GetBeforeUnloadReturnValue(JSContext* cx, unsigned argc, JS::Value* vp) {
  return Slot(cx, JS::CallArgsFromVp(argc, vp), "returnValue", Iface::BeforeUnload,
              kEventBeforeUnloadText);
}
bool SetBeforeUnloadReturnValue(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::BeforeUnload);
  if (!event) return false;
  ReportEventAccess(cx, event, "BeforeUnloadEvent", "returnValue", int(ApiAccess::Write));
  JS::RootedString text(cx, JS::ToString(cx, args.get(0)));
  if (!text) return false;
  JS::UniqueChars encoded = JS_EncodeStringToUTF8(cx, text);
  if (!encoded) return false;
  JS::RootedObject object(cx, Self(args));
  if (!SetSlotText(cx, object, kEventBeforeUnloadText, encoded.get())) return false;
  args.rval().setUndefined();
  return true;
}

// WebIDL enum conversion of SecurityPolicyViolationEventInit.disposition: the
// member must be exactly "enforce" or "report", anything else is the TypeError
// the donor's FindEnumStringIndex reports
// (obj-jscall-check/dom/bindings/SecurityPolicyViolationEventBinding.cpp:160).
bool ReadDisposition(JSContext* cx, JS::HandleObject dict, std::string* out) {
  if (!dict) return true;
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, dict, "disposition", &value)) return false;
  if (value.isUndefined()) return true;
  JS::RootedString text(cx, JS::ToString(cx, value));
  if (!text) return false;
  JS::UniqueChars encoded = JS_EncodeStringToUTF8(cx, text);
  if (!encoded) return false;
  std::string member(encoded.get());
  if (member != "enforce" && member != "report")
    return Error(cx, "TypeError",
                 "SecurityPolicyViolationEventInit.disposition is not a valid enum value");
  *out = member;
  return true;
}
}  // namespace

extern const JSPropertySpec kPageTransitionProperties[] = {
  JS_PSG("persisted", GetPagePersisted, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSPropertySpec kPromiseRejectionProperties[] = {
  JS_PSG("promise", GetRejectedPromise, JSPROP_ENUMERATE),
  JS_PSG("reason", GetRejectionReason, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSPropertySpec kSubmitProperties[] = {
  JS_PSG("submitter", GetSubmitter, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSPropertySpec kClipboardProperties[] = {
  JS_PSG("clipboardData", GetClipboardData, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSPropertySpec kTransitionProperties[] = {
  JS_PSG("propertyName", GetTransitionProperty, JSPROP_ENUMERATE),
  JS_PSG("elapsedTime", GetTransitionElapsed, JSPROP_ENUMERATE),
  JS_PSG("pseudoElement", GetTransitionPseudo, JSPROP_ENUMERATE),
  JS_PSG("animation", GetTransitionAnimation, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSPropertySpec kAnimationProperties[] = {
  JS_PSG("animationName", GetAnimationName, JSPROP_ENUMERATE),
  JS_PSG("elapsedTime", GetAnimationElapsed, JSPROP_ENUMERATE),
  JS_PSG("pseudoElement", GetAnimationPseudo, JSPROP_ENUMERATE),
  JS_PSG("animation", GetAnimationObject, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSPropertySpec kPolicyViolationProperties[] = {
  JS_PSG("documentURI", GetPolicyDocumentUri, JSPROP_ENUMERATE),
  JS_PSG("referrer", GetPolicyReferrer, JSPROP_ENUMERATE),
  JS_PSG("blockedURI", GetPolicyBlockedUri, JSPROP_ENUMERATE),
  JS_PSG("violatedDirective", GetPolicyViolatedDirective, JSPROP_ENUMERATE),
  JS_PSG("effectiveDirective", GetPolicyEffectiveDirective, JSPROP_ENUMERATE),
  JS_PSG("originalPolicy", GetPolicyOriginalPolicy, JSPROP_ENUMERATE),
  JS_PSG("sourceFile", GetPolicySourceFile, JSPROP_ENUMERATE),
  JS_PSG("sample", GetPolicySample, JSPROP_ENUMERATE),
  JS_PSG("disposition", GetPolicyDisposition, JSPROP_ENUMERATE),
  JS_PSG("statusCode", GetPolicyStatusCode, JSPROP_ENUMERATE),
  JS_PSG("lineNumber", GetPolicyLineNumber, JSPROP_ENUMERATE),
  JS_PSG("columnNumber", GetPolicyColumnNumber, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSPropertySpec kMediaQueryListProperties[] = {
  JS_PSG("media", GetMediaQueryMedia, JSPROP_ENUMERATE),
  JS_PSG("matches", GetMediaQueryMatches, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSPropertySpec kBeforeUnloadProperties[] = {
  JS_PSGS("returnValue", GetBeforeUnloadReturnValue, SetBeforeUnloadReturnValue,
          JSPROP_ENUMERATE),
  JS_PS_END
};

// Dictionary adapter of the nine interfaces above. The base EventInit members
// (bubbles/cancelable/composed) were already read by the shared constructor
// path in event_derived.cpp; every field below lands in the traced slots this
// file's accessors read back. Missing members keep the spec defaults.
bool InitPagePayloadFields(JSContext* cx, Iface iface, JS::HandleObject dict,
                           JS::HandleObject object) {
  switch (iface) {
    case Iface::PageTransition: {
      bool persisted = false;
      if (dict && !ReadInitBool(cx, dict, "persisted", &persisted)) return false;
      SetSlotBool(object, kEventPagePersisted, persisted);
      return true;
    }
    case Iface::PromiseRejection: {
      JS::RootedValue promise(cx, JS::NullValue());
      JS::RootedValue reason(cx, JS::UndefinedValue());
      if (dict && (!ReadInitValue(cx, dict, "promise", &promise) ||
                   !ReadInitValue(cx, dict, "reason", &reason))) return false;
      if (!promise.isObject())
        return Error(cx, "TypeError", "PromiseRejectionEventInit.promise is required");
      JS::SetReservedSlot(object, kEventPromise, promise);
      JS::SetReservedSlot(object, kEventRejectionReason, reason);
      return true;
    }
    case Iface::Submit: {
      JS::RootedValue submitter(cx);
      if (!ReadNullableObject(cx, dict, "submitter", &submitter)) return false;
      JS::SetReservedSlot(object, kEventSubmitter, submitter);
      return true;
    }
    case Iface::Clipboard: {
      std::string data, data_type;
      if (dict && (!ReadInitString(cx, dict, "data", &data) ||
                   !ReadInitString(cx, dict, "dataType", &data_type))) return false;
      return true;
    }
    case Iface::Transition: {
      std::string property, pseudo;
      double elapsed = 0.0;
      JS::RootedValue animation(cx);
      if (dict && (!ReadInitString(cx, dict, "propertyName", &property) ||
                   !ReadInitNumber(cx, dict, "elapsedTime", &elapsed) ||
                   !ReadInitString(cx, dict, "pseudoElement", &pseudo))) return false;
      if (!ReadNullableObject(cx, dict, "animation", &animation)) return false;
      JS::SetReservedSlot(object, kEventTransitionAnimation, animation);
      // elapsedTime is an IDL float: the donor rounds to float32 before storing.
      SetSlotNumber(object, kEventTransitionElapsed, double(float(elapsed)));
      return SetSlotText(cx, object, kEventTransitionProperty, property) &&
             SetSlotText(cx, object, kEventTransitionPseudo, pseudo);
    }
    case Iface::Animation: {
      std::string name, pseudo;
      double elapsed = 0.0;
      JS::RootedValue animation(cx);
      if (dict && (!ReadInitString(cx, dict, "animationName", &name) ||
                   !ReadInitNumber(cx, dict, "elapsedTime", &elapsed) ||
                   !ReadInitString(cx, dict, "pseudoElement", &pseudo))) return false;
      if (!ReadNullableObject(cx, dict, "animation", &animation)) return false;
      JS::SetReservedSlot(object, kEventAnimationObject, animation);
      SetSlotNumber(object, kEventAnimationElapsed, double(float(elapsed)));
      return SetSlotText(cx, object, kEventAnimationName, name) &&
             SetSlotText(cx, object, kEventAnimationPseudo, pseudo);
    }
    case Iface::PolicyViolation: {
      std::string document_uri, referrer, blocked_uri, violated, effective;
      std::string original, source_file, sample, disposition = "enforce";
      int status = 0, line = 0, column = 0;
      if (dict && (!ReadInitString(cx, dict, "documentURI", &document_uri) ||
                   !ReadInitString(cx, dict, "referrer", &referrer) ||
                   !ReadInitString(cx, dict, "blockedURI", &blocked_uri) ||
                   !ReadInitString(cx, dict, "violatedDirective", &violated) ||
                   !ReadInitString(cx, dict, "effectiveDirective", &effective) ||
                   !ReadInitString(cx, dict, "originalPolicy", &original) ||
                   !ReadInitString(cx, dict, "sourceFile", &source_file) ||
                   !ReadInitString(cx, dict, "sample", &sample) ||
                   !ReadInitInt(cx, dict, "statusCode", &status) ||
                   !ReadInitInt(cx, dict, "lineNumber", &line) ||
                   !ReadInitInt(cx, dict, "columnNumber", &column))) return false;
      if (!ReadDisposition(cx, dict, &disposition)) return false;
      SetSlotInt(object, kEventPolicyStatusCode, status);
      SetSlotInt(object, kEventPolicyLineNumber, line);
      SetSlotInt(object, kEventPolicyColumnNumber, column);
      return SetSlotText(cx, object, kEventPolicyDocumentUri, document_uri) &&
             SetSlotText(cx, object, kEventPolicyReferrer, referrer) &&
             SetSlotText(cx, object, kEventPolicyBlockedUri, blocked_uri) &&
             SetSlotText(cx, object, kEventPolicyViolatedDirective, violated) &&
             SetSlotText(cx, object, kEventPolicyEffectiveDirective, effective) &&
             SetSlotText(cx, object, kEventPolicyOriginalPolicy, original) &&
             SetSlotText(cx, object, kEventPolicySourceFile, source_file) &&
             SetSlotText(cx, object, kEventPolicySample, sample) &&
             SetSlotText(cx, object, kEventPolicyDisposition, disposition);
    }
    case Iface::MediaQueryList: {
      std::string media;
      bool matches = false;
      if (dict && (!ReadInitString(cx, dict, "media", &media) ||
                   !ReadInitBool(cx, dict, "matches", &matches))) return false;
      SetSlotBool(object, kEventMediaQueryMatches, matches);
      return SetSlotText(cx, object, kEventMediaQueryMedia, media);
    }
    case Iface::BeforeUnload: {
      // No constructor reaches this interface (its native throws Illegal
      // constructor) and the runtime does not fire beforeunload yet; the case
      // initializes the attribute to the donor's empty default so a future
      // lifecycle owner gets the same starting text.
      return SetSlotText(cx, object, kEventBeforeUnloadText, "");
    }
    default:
      return true;
  }
}
}  // namespace ruyijs::interaction
