// The named event interfaces beyond the Event base. Each one is a real
// realm-local constructor whose prototype chains onto its parent interface, so
// instanceof, the prototype walk and the read-only accessors follow the
// specification instead of a synthesized shape. TouchEvent is installed only
// when the Browser Environment reports touch capability, matching Firefox's
// TouchEvent::PrefEnabled. The subclass brand lives in the kind slot written by
// the constructor; dispatch still runs through the one interaction tree owned by
// dispatch.cpp.
#include "interaction/internal.h"
#include "interaction/event_keyboard.h"
#include "interaction/event_creation.h"
#include "interaction/touch.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "js/GlobalObject.h"
#include "js/PropertyAndElement.h"
#include "js/Object.h"
#include "js/Realm.h"
#include <chrono>
#include <string>

namespace ruyijs::interaction {
bool IsA(Kind actual, Iface required) {
  switch (required) {
    case Iface::Event:
      return actual != Kind::Speech;
    case Iface::Custom:
      return actual == Kind::Custom;
    case Iface::UI:
      return actual == Kind::UI || actual == Kind::Mouse || actual == Kind::Pointer ||
             actual == Kind::Keyboard || actual == Kind::Focus ||
             actual == Kind::Input || actual == Kind::Wheel || actual == Kind::Touch ||
             actual == Kind::Drag || actual == Kind::Composition || actual == Kind::TextInput ||
             actual == Kind::MouseScroll || actual == Kind::ScrollArea;
    case Iface::Mouse:
      return actual == Kind::Mouse || actual == Kind::Pointer || actual == Kind::Wheel ||
             actual == Kind::Drag || actual == Kind::MouseScroll;
    case Iface::Pointer:
      return actual == Kind::Pointer;
    case Iface::Keyboard:
      return actual == Kind::Keyboard;
    case Iface::Focus:
      return actual == Kind::Focus;
    case Iface::Input:
      return actual == Kind::Input;
    case Iface::Wheel:
      return actual == Kind::Wheel;
    case Iface::Touch:
      return actual == Kind::Touch;
    case Iface::Progress:
      return actual == Kind::Progress;
    case Iface::Message:
      return actual == Kind::Message;
    case Iface::Error:
      return actual == Kind::Error;
    case Iface::Close:
      return actual == Kind::Close;
    case Iface::Storage:
      return actual == Kind::Storage;
    case Iface::PopState:
      return actual == Kind::PopState;
    case Iface::HashChange:
      return actual == Kind::HashChange;
    case Iface::OfflineAudioComplete:
      return actual == Kind::OfflineAudioComplete;
    case Iface::PageTransition:
      return actual == Kind::PageTransition;
    case Iface::PromiseRejection:
      return actual == Kind::PromiseRejection;
    case Iface::Submit:
      return actual == Kind::Submit;
    case Iface::Drag:
      return actual == Kind::Drag;
    case Iface::Clipboard:
      return actual == Kind::Clipboard;
    case Iface::Composition:
      return actual == Kind::Composition;
    case Iface::Transition:
      return actual == Kind::Transition;
    case Iface::Animation:
      return actual == Kind::Animation;
    case Iface::PolicyViolation:
      return actual == Kind::PolicyViolation;
    case Iface::MediaQueryList:
      return actual == Kind::MediaQueryList;
    case Iface::BeforeUnload:
      return actual == Kind::BeforeUnload;
    case Iface::DeviceMotion:
      return actual == Kind::DeviceMotion;
    case Iface::DeviceOrientation:
      return actual == Kind::DeviceOrientation;
    case Iface::RtcIce:
      return actual == Kind::RtcIce;
    case Iface::RtcDataChannel:
      return actual == Kind::RtcDataChannel;
    case Iface::RtcTrack:
      return actual == Kind::RtcTrack;
    case Iface::AnimationPlayback:
      return actual == Kind::AnimationPlayback;
    case Iface::BlobPayload:
      return actual == Kind::BlobPayload;
    case Iface::ContentVisibility:
      return actual == Kind::ContentVisibility;
    case Iface::FormDataPayload:
      return actual == Kind::FormDataPayload;
    case Iface::Toggle:
      return actual == Kind::Toggle;
    case Iface::Gamepad:
      return actual == Kind::Gamepad;
    case Iface::TextInput:
      return actual == Kind::TextInput;
    case Iface::NavigationEntryChange:
      return actual == Kind::NavigationEntryChange;
    case Iface::Speech:
      return actual == Kind::Speech || actual == Kind::SpeechError || actual == Kind::SpeechTrusted;
    case Iface::SpeechError:
      return actual == Kind::SpeechError;
    case Iface::MidiConnection:
      return actual == Kind::MidiConnection;
    case Iface::MidiMessage:
      return actual == Kind::MidiMessage;
    case Iface::CookieChange:
      return actual == Kind::CookieChange;
    case Iface::PictureInPicture:
      return actual == Kind::PictureInPicture;
    case Iface::Navigate:
      return actual == Kind::Navigate;
    case Iface::MediaEncrypted:
      return actual == Kind::MediaEncrypted;
    case Iface::MediaKeyMessage:
      return actual == Kind::MediaKeyMessage;
    case Iface::MediaStream:
      return actual == Kind::MediaStream;
    case Iface::MediaTrack:
      return actual == Kind::MediaTrack;
    case Iface::RtcTone:
      return actual == Kind::RtcTone;
    case Iface::RtcErrorPayload:
      return actual == Kind::RtcErrorPayload;
    case Iface::RtcIceError:
      return actual == Kind::RtcIceError;
    case Iface::AudioProcessing:
      return actual == Kind::AudioProcessing;
    case Iface::FontLoad:
      return actual == Kind::FontLoad;
    case Iface::GpuUncaptured:
      return actual == Kind::GpuUncaptured;
    case Iface::TaskPriority:
      return actual == Kind::TaskPriority;
    case Iface::IdbVersionChange:
      return actual == Kind::IdbVersionChange;
    case Iface::DocumentPicture:
      return false;
    case Iface::PopupBlocked: return actual == Kind::PopupBlocked;
    case Iface::TimeEvent: return actual == Kind::TimeEvent;
    case Iface::MouseScroll: return actual == Kind::MouseScroll;
    case Iface::ScrollArea: return actual == Kind::ScrollArea;
    case Iface::Command: return actual == Kind::Command;
    case Iface::MediaStreamTrack: return actual == Kind::MediaStreamTrack;
    case Iface::MediaRecorderError: return actual == Kind::MediaRecorderError;
    case Iface::Extendable:
      return actual == Kind::Extendable || actual == Kind::ExtendableMessage || actual == Kind::Fetch;
    case Iface::ExtendableMessage: return actual == Kind::ExtendableMessage;
    case Iface::Fetch: return actual == Kind::Fetch;
    case Iface::RtcTransform: return actual == Kind::RtcTransform;
  }
  return false;
}

namespace {
bool Construct(JSContext* cx, const JS::CallArgs& args, Iface iface, Kind kind) {
  NativeConstructorTrace trace(cx, args);
  const bool public_event = IsPublicEvent(iface);
  JS::RootedObject touch_prototype(cx);
  if (iface == Iface::Touch && args.isConstructing() &&
      !TouchConstructorPrototype(cx, args, true, &touch_prototype)) return false;
  if (public_event && !args.isConstructing())
    return trace.Finish(CallPublicEvent(cx, args, iface));
  if (!args.isConstructing() || args.length() < PublicEventArity(iface))
    return Error(cx, "TypeError", "Event requires new and a type");
  State* state = activeState;
  while (state && state->page != JS::CurrentGlobalOrNull(cx)) state = state->previous;
  if (!state) return Error(cx, "TypeError", "Event realm is not installed");
  state->Observe(InterfaceName(iface), "constructor", int(ApiAccess::Construct));
  if (state->events.size() >= 100000) return Error(cx, "RangeError", "Event limit exceeded");
  JS::RootedString type(cx);
  std::string key;
  if (iface==Iface::TaskPriority ? !Text(cx,args[0],&type,&key) : !(iface == Iface::SpeechError || iface == Iface::PictureInPicture || iface == Iface::Navigate ||
        iface == Iface::MediaEncrypted || iface == Iface::MediaStream || iface == Iface::MediaTrack ||
        IsRtcExtraEvent(iface) || iface == Iface::AudioProcessing ||
        iface == Iface::FontLoad || iface == Iface::TaskPriority || iface == Iface::IdbVersionChange
        ? PublicFormattedEventType(cx, args[0], &type, &key) :
        public_event ? PublicEventType(cx, args[0], &type, &key)
                     : Text(cx, args[0], &type, &key))) return false;
  bool bubbles = false, cancelable = false, composed = false;
  if (!(public_event ? ReadPublicEventBase(cx, iface, args.get(1), &bubbles, &cancelable, &composed)
                     : ReadEventBase(cx, args.get(1), &bubbles, &cancelable, &composed)))
    return false;
  JS::RootedObject object(cx, iface == Iface::Touch
      ? JS_NewObjectWithGivenProto(cx, &kEventClass, touch_prototype)
      : JS_NewObjectForConstructor(cx, &kEventClass, args));
  if (!object) return false;
  JS::SetReservedSlot(object, kEventKind, JS::Int32Value(static_cast<int32_t>(kind)));
  if (!InstallTrusted(*state, object)) return false;
  if (!InitEventFields(cx, iface, args.get(1), object)) return false;
  auto event = std::make_unique<Event>(cx, object, type, std::move(key), bubbles, cancelable);
  event->kind = kind;
  event->value.set_composed(composed);
  event->value.set_time_stamp(std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - state->started).count());
  state->events.push_back(std::move(event));
  args.rval().setObject(*object);
  return trace.Finish(true);
}

// BeforeUnloadEvent has no WebIDL constructor: its interface object is the
// donor's ThrowingConstructor, which reports the TypeError the donor's
// Errors.msg:40 holds for MSG_ILLEGAL_CONSTRUCTOR
// (dom/bindings/BindingUtils.cpp:1540-1544).
bool IllegalConstructor(JSContext* cx, unsigned, JS::Value*) {
  return Error(cx, "TypeError", "Illegal constructor.");
}

#define EVENT_CONSTRUCTOR(name, iface, kind)                        \
  bool name(JSContext* cx, unsigned argc, JS::Value* vp) {           \
    return Construct(cx, JS::CallArgsFromVp(argc, vp), iface, kind); \
  }
EVENT_CONSTRUCTOR(ExtendableConstructor, Iface::Extendable, Kind::Extendable)
EVENT_CONSTRUCTOR(ExtendableMessageConstructor, Iface::ExtendableMessage, Kind::ExtendableMessage)
EVENT_CONSTRUCTOR(FetchConstructor, Iface::Fetch, Kind::Fetch)
EVENT_CONSTRUCTOR(CustomConstructor, Iface::Custom, Kind::Custom)
EVENT_CONSTRUCTOR(UiConstructor, Iface::UI, Kind::UI)
EVENT_CONSTRUCTOR(MouseConstructor, Iface::Mouse, Kind::Mouse)
EVENT_CONSTRUCTOR(PointerConstructor, Iface::Pointer, Kind::Pointer)
EVENT_CONSTRUCTOR(KeyboardConstructor, Iface::Keyboard, Kind::Keyboard)
EVENT_CONSTRUCTOR(FocusConstructor, Iface::Focus, Kind::Focus)
EVENT_CONSTRUCTOR(InputConstructor, Iface::Input, Kind::Input)
EVENT_CONSTRUCTOR(WheelConstructor, Iface::Wheel, Kind::Wheel)
EVENT_CONSTRUCTOR(TouchConstructor, Iface::Touch, Kind::Touch)
EVENT_CONSTRUCTOR(ProgressConstructor, Iface::Progress, Kind::Progress)
EVENT_CONSTRUCTOR(MessageConstructor, Iface::Message, Kind::Message)
EVENT_CONSTRUCTOR(ErrorConstructor, Iface::Error, Kind::Error)
EVENT_CONSTRUCTOR(CloseConstructor, Iface::Close, Kind::Close)
EVENT_CONSTRUCTOR(StorageConstructor, Iface::Storage, Kind::Storage)
EVENT_CONSTRUCTOR(PopStateConstructor, Iface::PopState, Kind::PopState)
EVENT_CONSTRUCTOR(HashChangeConstructor, Iface::HashChange, Kind::HashChange)
EVENT_CONSTRUCTOR(OfflineAudioCompleteConstructor, Iface::OfflineAudioComplete,
                  Kind::OfflineAudioComplete)
EVENT_CONSTRUCTOR(PageTransitionConstructor, Iface::PageTransition,
                  Kind::PageTransition)
EVENT_CONSTRUCTOR(PromiseRejectionConstructor, Iface::PromiseRejection,
                  Kind::PromiseRejection)
EVENT_CONSTRUCTOR(SubmitConstructor, Iface::Submit, Kind::Submit)
EVENT_CONSTRUCTOR(DragConstructor, Iface::Drag, Kind::Drag)
EVENT_CONSTRUCTOR(ClipboardConstructor, Iface::Clipboard, Kind::Clipboard)
EVENT_CONSTRUCTOR(CompositionConstructor, Iface::Composition, Kind::Composition)
EVENT_CONSTRUCTOR(TransitionConstructor, Iface::Transition, Kind::Transition)
EVENT_CONSTRUCTOR(AnimationConstructor, Iface::Animation, Kind::Animation)
EVENT_CONSTRUCTOR(PolicyViolationConstructor, Iface::PolicyViolation,
                  Kind::PolicyViolation)
EVENT_CONSTRUCTOR(MediaQueryListConstructor, Iface::MediaQueryList,
                  Kind::MediaQueryList)
EVENT_CONSTRUCTOR(DeviceMotionConstructor, Iface::DeviceMotion,
                  Kind::DeviceMotion)
EVENT_CONSTRUCTOR(DeviceOrientationConstructor, Iface::DeviceOrientation,
                  Kind::DeviceOrientation)
EVENT_CONSTRUCTOR(RtcIceConstructor, Iface::RtcIce, Kind::RtcIce)
EVENT_CONSTRUCTOR(RtcDataChannelConstructor, Iface::RtcDataChannel,
                  Kind::RtcDataChannel)
EVENT_CONSTRUCTOR(RtcTrackConstructor, Iface::RtcTrack, Kind::RtcTrack)
EVENT_CONSTRUCTOR(AnimationPlaybackConstructor, Iface::AnimationPlayback, Kind::AnimationPlayback)
EVENT_CONSTRUCTOR(BlobPayloadConstructor, Iface::BlobPayload, Kind::BlobPayload)
EVENT_CONSTRUCTOR(ContentVisibilityConstructor, Iface::ContentVisibility, Kind::ContentVisibility)
EVENT_CONSTRUCTOR(FormDataPayloadConstructor, Iface::FormDataPayload, Kind::FormDataPayload)
EVENT_CONSTRUCTOR(ToggleConstructor, Iface::Toggle, Kind::Toggle)
EVENT_CONSTRUCTOR(GamepadConstructor, Iface::Gamepad, Kind::Gamepad)
EVENT_CONSTRUCTOR(TextInputConstructor, Iface::TextInput, Kind::TextInput)
EVENT_CONSTRUCTOR(NavigationEntryChangeConstructor, Iface::NavigationEntryChange,
                  Kind::NavigationEntryChange)
EVENT_CONSTRUCTOR(SpeechConstructor, Iface::Speech, Kind::Speech)
EVENT_CONSTRUCTOR(SpeechErrorConstructor, Iface::SpeechError, Kind::SpeechError)
EVENT_CONSTRUCTOR(MidiConnectionConstructor, Iface::MidiConnection, Kind::MidiConnection)
EVENT_CONSTRUCTOR(MidiMessageConstructor, Iface::MidiMessage, Kind::MidiMessage)
EVENT_CONSTRUCTOR(CookieChangeConstructor, Iface::CookieChange, Kind::CookieChange)
EVENT_CONSTRUCTOR(PictureInPictureConstructor, Iface::PictureInPicture, Kind::PictureInPicture)
EVENT_CONSTRUCTOR(NavigateConstructor, Iface::Navigate, Kind::Navigate)
EVENT_CONSTRUCTOR(MediaEncryptedConstructor, Iface::MediaEncrypted, Kind::MediaEncrypted)
EVENT_CONSTRUCTOR(MediaKeyMessageConstructor, Iface::MediaKeyMessage, Kind::MediaKeyMessage)
EVENT_CONSTRUCTOR(MediaStreamConstructor, Iface::MediaStream, Kind::MediaStream)
EVENT_CONSTRUCTOR(MediaTrackConstructor, Iface::MediaTrack, Kind::MediaTrack)
EVENT_CONSTRUCTOR(RtcToneConstructor, Iface::RtcTone, Kind::RtcTone)
EVENT_CONSTRUCTOR(RtcErrorPayloadConstructor, Iface::RtcErrorPayload, Kind::RtcErrorPayload)
EVENT_CONSTRUCTOR(RtcIceErrorConstructor, Iface::RtcIceError, Kind::RtcIceError)
EVENT_CONSTRUCTOR(AudioProcessingConstructor, Iface::AudioProcessing, Kind::AudioProcessing)
EVENT_CONSTRUCTOR(FontLoadConstructor, Iface::FontLoad, Kind::FontLoad)
EVENT_CONSTRUCTOR(GpuUncapturedConstructor, Iface::GpuUncaptured, Kind::GpuUncaptured)
EVENT_CONSTRUCTOR(TaskPriorityConstructor, Iface::TaskPriority, Kind::TaskPriority)
EVENT_CONSTRUCTOR(IdbVersionChangeConstructor, Iface::IdbVersionChange, Kind::IdbVersionChange)
#undef EVENT_CONSTRUCTOR

struct Interface {
  const char* name;
  const char* parent;
  Iface iface;
  Kind kind;
  JSNative constructor;
  const JSPropertySpec* properties;
  const JSFunctionSpec* methods;
  const EventConstant* constants;
};

// Parents precede children so the parent prototype is installed when it is read.
const Interface kInterfaces[] = {
  {"CustomEvent", "Event", Iface::Custom, Kind::Custom, CustomConstructor,
   kCustomProperties, kCustomMethods, nullptr},
  {"UIEvent", "Event", Iface::UI, Kind::UI, UiConstructor,
   kUiProperties, kUiMethods, nullptr},
  {"MouseEvent", "UIEvent", Iface::Mouse, Kind::Mouse, MouseConstructor,
   kMouseProperties, kMouseMethods, kMouseConstants},
  {"PointerEvent", "MouseEvent", Iface::Pointer, Kind::Pointer, PointerConstructor,
   kPointerProperties, kPointerMethods, nullptr},
  {"KeyboardEvent", "UIEvent", Iface::Keyboard, Kind::Keyboard, KeyboardConstructor,
   kKeyboardProperties, kKeyboardMethods, kKeyboardConstants},
  {"FocusEvent", "UIEvent", Iface::Focus, Kind::Focus, FocusConstructor,
   kFocusProperties, nullptr, nullptr},
  {"InputEvent", "UIEvent", Iface::Input, Kind::Input, InputConstructor,
   kInputProperties, kInputMethods, nullptr},
  {"WheelEvent", "MouseEvent", Iface::Wheel, Kind::Wheel, WheelConstructor,
   kWheelProperties, nullptr, kWheelConstants},
  {"TouchEvent", "UIEvent", Iface::Touch, Kind::Touch, TouchConstructor,
   kTouchProperties, kTouchEventMethods, nullptr},
  {"ProgressEvent", "Event", Iface::Progress, Kind::Progress, ProgressConstructor,
   kProgressProperties, nullptr, nullptr},
  {"MessageEvent", "Event", Iface::Message, Kind::Message, MessageConstructor,
   kMessageProperties, kMessageMethods, nullptr},
  {"ErrorEvent", "Event", Iface::Error, Kind::Error, ErrorConstructor,
   kErrorProperties, nullptr, nullptr},
  {"CloseEvent", "Event", Iface::Close, Kind::Close, CloseConstructor,
   kCloseProperties, nullptr, nullptr},
  {"StorageEvent", "Event", Iface::Storage, Kind::Storage, StorageConstructor,
   kStorageProperties, kStorageMethods, nullptr},
  // The T9/T10 navigation events. The History and Location owners fire these
  // trusted events; their payload tables live in event_navigation.cpp.
  {"PopStateEvent", "Event", Iface::PopState, Kind::PopState, PopStateConstructor,
   kPopStateProperties, nullptr, nullptr},
  {"HashChangeEvent", "Event", Iface::HashChange, Kind::HashChange, HashChangeConstructor,
   kHashChangeProperties, nullptr, nullptr},
  // The Web Audio render completion (T13c). The OfflineAudioContext owner fires
  // this trusted event carrying the buffer it rendered.
  {"OfflineAudioCompletionEvent", "Event", Iface::OfflineAudioComplete,
   Kind::OfflineAudioComplete, OfflineAudioCompleteConstructor,
   kOfflineAudioCompleteProperties, nullptr, nullptr},
  // The T3c event subclasses. Their payload tables are split across four TUs
  // (event_supplemental.cpp, event_input_extra.cpp, event_device.cpp,
  // event_rtc.cpp) so no table TU carries more than one class cluster.
  {"PageTransitionEvent", "Event", Iface::PageTransition, Kind::PageTransition,
   PageTransitionConstructor, kPageTransitionProperties, nullptr, nullptr},
  {"PromiseRejectionEvent", "Event", Iface::PromiseRejection,
   Kind::PromiseRejection, PromiseRejectionConstructor,
   kPromiseRejectionProperties, nullptr, nullptr},
  {"SubmitEvent", "Event", Iface::Submit, Kind::Submit, SubmitConstructor,
   kSubmitProperties, nullptr, nullptr},
  {"DragEvent", "MouseEvent", Iface::Drag, Kind::Drag, DragConstructor,
   kDragProperties, kDragMethods, nullptr},
  {"ClipboardEvent", "Event", Iface::Clipboard, Kind::Clipboard,
   ClipboardConstructor, kClipboardProperties, nullptr, nullptr},
  {"CompositionEvent", "UIEvent", Iface::Composition, Kind::Composition,
   CompositionConstructor, kCompositionProperties, kCompositionMethods, nullptr},
  {"TransitionEvent", "Event", Iface::Transition, Kind::Transition,
   TransitionConstructor, kTransitionProperties, nullptr, nullptr},
  {"AnimationEvent", "Event", Iface::Animation, Kind::Animation,
   AnimationConstructor, kAnimationProperties, nullptr, nullptr},
  {"SecurityPolicyViolationEvent", "Event", Iface::PolicyViolation,
   Kind::PolicyViolation, PolicyViolationConstructor, kPolicyViolationProperties,
   nullptr, nullptr},
  {"MediaQueryListEvent", "Event", Iface::MediaQueryList, Kind::MediaQueryList,
   MediaQueryListConstructor, kMediaQueryListProperties, nullptr, nullptr},
  // The one interface object without a constructor: calling or constructing it
  // reports the donor's "Illegal constructor." TypeError.
  {"BeforeUnloadEvent", "Event", Iface::BeforeUnload, Kind::BeforeUnload,
   IllegalConstructor, kBeforeUnloadProperties, nullptr, nullptr},
  {"DeviceMotionEvent", "Event", Iface::DeviceMotion, Kind::DeviceMotion,
   DeviceMotionConstructor, kDeviceMotionProperties, kDeviceMotionMethods,
   nullptr},
  {"DeviceOrientationEvent", "Event", Iface::DeviceOrientation,
   Kind::DeviceOrientation, DeviceOrientationConstructor,
   kDeviceOrientationProperties, kDeviceOrientationMethods, nullptr},
  {"RTCPeerConnectionIceEvent", "Event", Iface::RtcIce, Kind::RtcIce,
   RtcIceConstructor, kRtcIceProperties, nullptr, nullptr},
  {"RTCDataChannelEvent", "Event", Iface::RtcDataChannel, Kind::RtcDataChannel,
   RtcDataChannelConstructor, kRtcDataChannelProperties, nullptr, nullptr},
  {"RTCTrackEvent", "Event", Iface::RtcTrack, Kind::RtcTrack,
   RtcTrackConstructor, kRtcTrackProperties, nullptr, nullptr},
  {"AnimationPlaybackEvent", "Event", Iface::AnimationPlayback, Kind::AnimationPlayback,
   AnimationPlaybackConstructor, kAnimationPlaybackProperties, nullptr, nullptr},
  {"BlobEvent", "Event", Iface::BlobPayload, Kind::BlobPayload,
   BlobPayloadConstructor, kBlobPayloadProperties, nullptr, nullptr},
  {"ContentVisibilityAutoStateChangeEvent", "Event", Iface::ContentVisibility,
   Kind::ContentVisibility, ContentVisibilityConstructor, kContentVisibilityProperties,
   nullptr, nullptr},
  {"FormDataEvent", "Event", Iface::FormDataPayload, Kind::FormDataPayload,
   FormDataPayloadConstructor, kFormDataPayloadProperties, nullptr, nullptr},
  {"ToggleEvent", "Event", Iface::Toggle, Kind::Toggle,
   ToggleConstructor, kToggleProperties, nullptr, nullptr},
  {"GamepadEvent", "Event", Iface::Gamepad, Kind::Gamepad,
   GamepadConstructor, kGamepadProperties, nullptr, nullptr},
  {"TextEvent", "UIEvent", Iface::TextInput, Kind::TextInput,
   TextInputConstructor, kTextInputProperties, kTextInputMethods, nullptr},
  {"NavigationCurrentEntryChangeEvent", "Event", Iface::NavigationEntryChange,
   Kind::NavigationEntryChange, NavigationEntryChangeConstructor,
   kNavigationEntryChangeProperties, nullptr, nullptr},
  {"SpeechSynthesisEvent", "Event", Iface::Speech, Kind::Speech,
   SpeechConstructor, kSpeechProperties, nullptr, nullptr},
  {"SpeechSynthesisErrorEvent", "SpeechSynthesisEvent", Iface::SpeechError, Kind::SpeechError,
   SpeechErrorConstructor, kSpeechErrorProperties, nullptr, nullptr},
  {"MIDIConnectionEvent", "Event", Iface::MidiConnection, Kind::MidiConnection,
   MidiConnectionConstructor, kMidiConnectionProperties, nullptr, nullptr},
  {"MIDIMessageEvent", "Event", Iface::MidiMessage, Kind::MidiMessage,
   MidiMessageConstructor, kMidiMessageProperties, nullptr, nullptr},
  {"CookieChangeEvent", "Event", Iface::CookieChange, Kind::CookieChange,
   CookieChangeConstructor, kCookieChangeProperties, nullptr, nullptr},
  {"PictureInPictureEvent", "Event", Iface::PictureInPicture, Kind::PictureInPicture,
   PictureInPictureConstructor, kPictureInPictureProperties, nullptr, nullptr},
  {"NavigateEvent", "Event", Iface::Navigate, Kind::Navigate,
   NavigateConstructor, kNavigateProperties, kNavigateMethods, nullptr},
  {"MediaEncryptedEvent", "Event", Iface::MediaEncrypted, Kind::MediaEncrypted,
   MediaEncryptedConstructor, kMediaEncryptedProperties, nullptr, nullptr},
  {"MediaKeyMessageEvent", "Event", Iface::MediaKeyMessage, Kind::MediaKeyMessage,
   MediaKeyMessageConstructor, kMediaKeyMessageProperties, nullptr, nullptr},
  {"MediaStreamEvent", "Event", Iface::MediaStream, Kind::MediaStream,
   MediaStreamConstructor, kMediaStreamProperties, nullptr, nullptr},
  {"TrackEvent", "Event", Iface::MediaTrack, Kind::MediaTrack,
   MediaTrackConstructor, kMediaTrackProperties, nullptr, nullptr},
  {"RTCDTMFToneChangeEvent", "Event", Iface::RtcTone, Kind::RtcTone,
   RtcToneConstructor, kRtcToneProperties, nullptr, nullptr},
  {"RTCErrorEvent", "Event", Iface::RtcErrorPayload, Kind::RtcErrorPayload,
   RtcErrorPayloadConstructor, kRtcErrorPayloadProperties, nullptr, nullptr},
  {"RTCPeerConnectionIceErrorEvent", "Event", Iface::RtcIceError, Kind::RtcIceError,
   RtcIceErrorConstructor, kRtcIceErrorProperties, nullptr, nullptr},
  {"AudioProcessingEvent", "Event", Iface::AudioProcessing, Kind::AudioProcessing,
   AudioProcessingConstructor, kAudioProcessingProperties, nullptr, nullptr},
  {"FontFaceSetLoadEvent", "Event", Iface::FontLoad, Kind::FontLoad,
   FontLoadConstructor, kFontLoadProperties, nullptr, nullptr},
  {"GPUUncapturedErrorEvent", "Event", Iface::GpuUncaptured, Kind::GpuUncaptured,
   GpuUncapturedConstructor, kGpuUncapturedProperties, nullptr, nullptr},
  {"TaskPriorityChangeEvent", "Event", Iface::TaskPriority, Kind::TaskPriority,
   TaskPriorityConstructor, kTaskPriorityProperties, nullptr, nullptr},
  {"IDBVersionChangeEvent", "Event", Iface::IdbVersionChange, Kind::IdbVersionChange,
   IdbVersionChangeConstructor, kIdbVersionChangeProperties, nullptr, nullptr},
  {"PopupBlockedEvent", "Event", Iface::PopupBlocked, Kind::PopupBlocked, PopupBlockedConstructor,
   kPopupBlockedProperties, nullptr, nullptr},
  {"TimeEvent", "Event", Iface::TimeEvent, Kind::TimeEvent, IllegalConstructor,
   kTimeEventProperties, kTimeEventMethods, nullptr},
  {"MouseScrollEvent", "MouseEvent", Iface::MouseScroll, Kind::MouseScroll, IllegalConstructor,
   kMouseScrollProperties, kMouseScrollMethods, kMouseScrollConstants},
  {"ScrollAreaEvent", "UIEvent", Iface::ScrollArea, Kind::ScrollArea, IllegalConstructor,
   kScrollAreaProperties, kScrollAreaMethods, nullptr},
  {"CommandEvent", "Event", Iface::Command, Kind::Command, CommandConstructor,
   kCommandProperties, nullptr, nullptr},
  {"MediaStreamTrackEvent", "Event", Iface::MediaStreamTrack, Kind::MediaStreamTrack,
   MediaStreamTrackConstructor, kMediaStreamTrackProperties, nullptr, nullptr},
  {"MediaRecorderErrorEvent", "Event", Iface::MediaRecorderError, Kind::MediaRecorderError,
   MediaRecorderErrorConstructor, kMediaRecorderErrorProperties, nullptr, nullptr},
  // Service Worker events (event_extendable.cpp), ServiceWorkerGlobalScope only.
  {"ExtendableEvent", "Event", Iface::Extendable, Kind::Extendable, ExtendableConstructor,
   nullptr, kExtendableMethods, nullptr},
  {"ExtendableMessageEvent", "ExtendableEvent", Iface::ExtendableMessage, Kind::ExtendableMessage,
   ExtendableMessageConstructor, kExtendableMessageProperties, nullptr, nullptr},
  {"FetchEvent", "ExtendableEvent", Iface::Fetch, Kind::Fetch, FetchConstructor,
   kFetchProperties, kFetchMethods, nullptr},
  {"DocumentPictureInPictureEvent", "Event", Iface::DocumentPicture, Kind::Event,
   DocumentPictureConstructor, kDocumentPictureProperties, nullptr, nullptr},
  {"RTCTransformEvent", "Event", Iface::RtcTransform, Kind::RtcTransform,
   RTCTransformConstructor, kRtcTransformProperties, nullptr, nullptr},
};

bool DefineConstants(JSContext* cx, const EventConstant* constants,
                     JS::HandleObject prototype, JS::HandleObject constructor) {
  const unsigned flags = JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
  for (const EventConstant* entry = constants; entry && entry->name; ++entry) {
    JS::RootedValue value(cx, JS::Int32Value(entry->value));
    // The prototype copy is made permanent by the binding order pass, after it is placed last.
    if (!JS_DefineProperty(cx, prototype, entry->name, value, flags & ~JSPROP_PERMANENT) ||
        !JS_DefineProperty(cx, constructor, entry->name, value, flags)) return false;
  }
  return true;
}

bool RegisterMembers(State& state, const Interface& entry, JS::HandleObject prototype,
                     JS::HandleObject constructor) {
  JSContext* cx = state.cx;
  if (!state.helpers->Interface(entry.name, prototype)) return false;
  if (!state.helpers->Constructor(entry.name, constructor)) return false;
  for (const JSPropertySpec* property = entry.properties;
       property && property->name.string(); ++property) {
    if (!state.helpers->Accessor(entry.name, prototype, property->name.string()))
      return false;
  }
  for (const JSFunctionSpec* method = entry.methods; method && method->name.string();
       ++method) {
    JS::RootedValue value(cx);
    if (!JS_GetProperty(cx, prototype, method->name.string(), &value) || !value.isObject())
      return Error(cx, "TypeError", "Event method is not installed");
    JS::RootedObject function(cx, &value.toObject());
    if (!state.helpers->Member(function, entry.name, method->name.string(), "call"))
      return false;
  }
  return true;
}
}  // namespace

bool IsWorkerEvent(Iface iface) {
  switch (iface) {
    case Iface::Custom:
    case Iface::Progress:
    case Iface::Message:
    case Iface::Error:
    case Iface::Close:
    case Iface::PromiseRejection:
    case Iface::RtcErrorPayload:
    case Iface::FontLoad:
    case Iface::GpuUncaptured:
    case Iface::TaskPriority:
    case Iface::IdbVersionChange:
      return true;
    default:
      return false;
  }
}

// Events exposed only in a DedicatedWorker (not SharedWorker, not ServiceWorker).
// Firefox uses [Exposed=DedicatedWorker] for these interfaces.
bool IsDedicatedWorkerEvent(Iface iface) {
  switch (iface) {
    case Iface::RtcTransform:
      return true;
    default:
      return false;
  }
}

// True when the event interface is exposed to this global kind (Firefox [Exposed]).
bool Exposed(Iface iface, Exposure exposure) {
  switch (exposure) {
    case Exposure::Window:
      return !IsServiceWorkerEvent(iface) && !IsDedicatedWorkerEvent(iface);
    case Exposure::Worker:  // SharedWorker
      return IsWorkerEvent(iface) && iface != Iface::RtcErrorPayload;
    case Exposure::ServiceWorker:
      return iface != Iface::RtcErrorPayload &&
             (IsWorkerEvent(iface) || IsServiceWorkerEvent(iface));
    case Exposure::DedicatedWorker:
      if (IsServiceWorkerEvent(iface)) return false;
      return IsWorkerEvent(iface) || IsDedicatedWorkerEvent(iface);
  }
  return false;
}

bool InstallDerivedEvents(State& state, bool touch_enabled, Exposure exposure) {
  JSContext* cx = state.cx;
  // The [LegacyNoInterfaceObject] device helper prototypes install first: the
  // DeviceMotionEvent constructor materializes helper instances from them.
  if (exposure == Exposure::Window &&
      (!InstallSupplementalEventObjects(state) || !InstallTouchInterfaces(state))) return false;
  for (const Interface& entry : kInterfaces) {
    if (!Exposed(entry.iface, exposure)) continue;
    // Window Touch factories are created once and hidden/restored after their
    // original descriptors are captured. Workers filter them above.
    JS::RootedValue parent(cx);
    if (!JS_GetProperty(cx, state.page, entry.parent, &parent) || !parent.isObject())
      return Error(cx, "TypeError", "Event parent interface is not installed");
    JS::RootedObject parent_object(cx, &parent.toObject());
    JS::RootedValue prototype_value(cx);
    if (!JS_GetProperty(cx, parent_object, "prototype", &prototype_value) ||
        !prototype_value.isObject())
      return Error(cx, "TypeError", "Event parent prototype is not installed");
    JS::RootedObject parent_prototype(cx, &prototype_value.toObject());
    JS::RootedObject prototype(cx, JS_InitClass(cx, state.page, &kEventClass,
        parent_prototype, entry.name, entry.constructor, PublicEventArity(entry.iface),
        entry.iface == Iface::Keyboard || entry.iface == Iface::Touch || IsFirefoxPayloadEvent(entry.iface) ? nullptr : entry.properties,
        entry.iface == Iface::Keyboard || entry.iface == Iface::Touch || IsFirefoxPayloadEvent(entry.iface) ? nullptr : entry.methods, nullptr, nullptr));
    if (!prototype) return false;
    if ((entry.iface == Iface::Keyboard || entry.iface == Iface::Touch || IsFirefoxPayloadEvent(entry.iface)) &&
        ((entry.methods && !JS_DefineFunctions(cx, prototype, entry.methods)) ||
         (entry.properties && !JS_DefineProperties(cx, prototype, entry.properties)))) return false;
    if (!Tag(cx, prototype, entry.name) ||
        !SaveLegacyPrototype(state, entry.iface, prototype)) return false;
    if (entry.iface == Iface::Mouse) state.mousePrototype = prototype;
    if (entry.iface == Iface::Pointer) state.pointerPrototype = prototype;
    JS::RootedValue constructor_value(cx);
    if (!JS_GetProperty(cx, state.page, entry.name, &constructor_value) ||
        !constructor_value.isObject()) return false;
    JS::RootedObject constructor(cx, &constructor_value.toObject());
    // Keep the observed constructor-last string-key order within this installer.
    if ((entry.iface == Iface::Touch || entry.iface == Iface::NavigationEntryChange || entry.iface == Iface::Speech ||
         entry.iface == Iface::SpeechError || IsPeripheralEvent(entry.iface) || entry.iface == Iface::Navigate ||
         IsMediaPayloadEvent(entry.iface) || IsRtcExtraEvent(entry.iface) || IsProcessingEvent(entry.iface) ||
         entry.iface == Iface::IdbVersionChange || entry.iface == Iface::DocumentPicture) &&
        (!JS_DeleteProperty(cx, prototype, "constructor") ||
         !JS_DefineProperty(cx, prototype, "constructor", constructor_value, 0))) return false;
    // WebIDL: the interface object inherits from its parent interface object, so
    // MouseEvent.__proto__ is UIEvent and PointerEvent.__proto__ is MouseEvent.
    if ((!IsPublicEvent(entry.iface)||entry.iface==Iface::TaskPriority) && !JS_SetPrototype(cx, constructor, parent_object))
      return false;
    if (!DefineConstants(cx, entry.constants, prototype, constructor)) return false;
    if ((entry.iface == Iface::Keyboard || IsFirefoxPayloadEvent(entry.iface)) &&
        (!JS_DeleteProperty(cx, prototype, "constructor") ||
         !JS_DefineProperty(cx, prototype, "constructor", constructor_value, 0))) return false;
    if (!RegisterMembers(state, entry, prototype, constructor)) return false;
    if (!InstallEventCoordinates(state, entry.iface, prototype)) return false;
  }
  if (exposure != Exposure::Window) return true;
  return InstallKeyEvent(state) && InstallDataTransfer(state) &&
      CaptureTouchExposure(state) && ApplyTouchExposure(state, touch_enabled);
}
}  // namespace ruyijs::interaction
