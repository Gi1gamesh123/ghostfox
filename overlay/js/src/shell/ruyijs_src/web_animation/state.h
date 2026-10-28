#ifndef RUYIJS_WEB_ANIMATION_STATE_H
#define RUYIJS_WEB_ANIMATION_STATE_H
#include <optional>
#include <string>
#include "web_animation/realm.h"
#include "domain/animation/timing.h"
#include "runtime/web_kit.h"
#include "js/GCVector.h"
#include "js/Object.h"
#include "jsapi.h"

namespace ruyijs::web_animation {
namespace model = ::ruyijs::domain::animation;
// Slot 0 of every object of this owner is its State (the realm that made it).
enum AnimationSlot : uint32_t {
  kAOwner = 0, kAId, kAEffect, kATimeline, kAStart, kAHold, kARate, kAPendingRate,
  kAPendingPlay, kAPendingPause, kAPendingTask, kATimerTask, kAReady, kAFinished,
  kAReplace, kASequence, kAPrevious, kAOnFinish, kAOnCancel, kAOnRemove, kACount
};
enum EffectSlot : uint32_t {
  kEOwner = 0, kETarget, kEPseudo, kEComposite, kEIterComposite, kEDelay, kEEndDelay,
  kEIterStart, kEIterations, kEDuration, kEFill, kEDirection, kEEasing, kEFrames,
  kEAnimation, kECount
};
enum TimelineSlot : uint32_t { kTOwner = 0, kTDocument, kTOrigin, kTCount };
extern const JSClass kAnimationClass, kEffectClass, kTimelineClass, kCssAnimationClass,
    kCssTransitionClass;
extern const char* const kCompositeNames[3];  // replace, add, accumulate
extern const char* const kHandlerNames[3];    // finish, cancel, remove

struct State {
  State(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, interaction::Realm& events);
  JSContext* cx;
  WebKit kit;
  interaction::Realm& events;
  WebFace timeline, documentTimeline, effect, keyframeEffect, animation, cssAnimation, cssTransition;
  JS::PersistentRootedObject timelines;  // WeakMap Document -> DocumentTimeline
  JS::PersistentRootedValue playbackEvent, domException;
  JS::PersistentRooted<JS::GCVector<JSObject*, 0, js::SystemAllocPolicy>> relevant;
  int32_t sequence = 0;
  // The realm's sampled clock time (ms since the page's time origin). Like
  // Firefox's refresh-driver tick it holds still for the rest of the current
  // task; a posted task clears it at the next task boundary.
  std::optional<double> tick;
  Realm::DocumentPorts dom;
  Realm::SchedulingPorts scheduling;
};
State& OwnerOf(JSObject* object);
bool IsAnimation(JSObject* object);  // Animation, CSSAnimation or CSSTransition

// Errors (realm.cpp). Type errors carry Firefox's text; DOM errors use the
// page realm's own DOMException.
bool TypeFail(JSContext* cx, const std::string& message);
bool DomFail(State& s, const char* name, const std::string& message);
bool NotImplemented(JSContext* cx, const char* member, const char* iface);
bool RejectPending(State& s, JS::MutableHandleValue rval);  // pending exception -> rejected promise
std::string NumberText(JSContext* cx, double value);         // JS Number::toString
bool Text(JSContext* cx, JS::HandleValue value, std::string* out);
bool NewString(JSContext* cx, const std::string& text, JS::MutableHandleValue out);
JSObject* NewTask(State& s, JSNative native, JS::HandleObject target);  // a one-shot task function

// Timelines (timeline.cpp).
std::optional<double> TimelineTime(JSObject* timeline);  // null when inactive
bool DocumentTimeline(State& s, JS::HandleObject document, JS::MutableHandleObject result);
bool InstallTimelines(State& s, JS::HandleObject documentPrototype);

// Effect timing and keyframes (effect_timing.cpp, keyframes.cpp, keyframe_effect.cpp).
model::EffectTiming ReadTiming(JSObject* effect);
// Reads an EffectTiming-like dictionary or number into `timing`; `partial`
// keeps members the dictionary omits (updateTiming). Errors use `prefix`.
bool ParseTiming(JSContext* cx, JS::HandleValue input, bool partial, const char* dictionary,
                 const std::string& prefix, model::EffectTiming* timing);
bool StoreTiming(JSObject* effect, const model::EffectTiming& timing);
bool TimingObject(JSContext* cx, const model::EffectTiming& t, bool computed,
                  JSObject* effect, JS::MutableHandleValue result);
bool ParseKeyframes(JSContext* cx, JS::HandleValue input, const std::string& prefix,
                    JS::MutableHandleObject frames);
bool KeyframesObject(JSContext* cx, JS::HandleObject frames, JS::MutableHandleValue result);
bool NewKeyframeEffect(State& s, const JS::CallArgs& args, JS::HandleObject target,
                       JS::HandleValue keyframes, JS::HandleValue options, const char* dictionary,
                       const std::string& prefix, JS::MutableHandleObject effect);
bool InstallEffects(State& s);
JSObject* NewEffectObject(State& s, JS::HandleObject proto);  // effect_options.cpp
// A pseudo-element Firefox animates (::before, ::after, ::marker), canonicalized; SyntaxError otherwise.
bool PseudoElement(State& s, JS::HandleValue value, const std::string& prefix, JS::MutableHandleValue out);

// Animations (animation.cpp, animation_play.cpp).
std::optional<double> CurrentTime(JSObject* animation);
model::ComputedTiming AnimationTiming(JSObject* animation);
const char* PlayState(JSObject* animation);
bool NewAnimation(State& s, JS::HandleObject proto, JS::HandleObject effect,
                  JS::HandleObject timeline, JS::MutableHandleObject animation);
bool SetEffect(State& s, JS::HandleObject animation, JS::HandleObject effect);
bool Play(State& s, JS::HandleObject animation, bool autoRewind, const char* prefix);
bool Pause(State& s, JS::HandleObject animation, const char* prefix);
bool Finish(State& s, JS::HandleObject animation);
bool Cancel(State& s, JS::HandleObject animation);
bool Reverse(State& s, JS::HandleObject animation);
bool SetCurrentTime(State& s, JS::HandleObject animation, JS::HandleValue value);
bool SetStartTime(State& s, JS::HandleObject animation, JS::HandleValue value);
bool SetPlaybackRate(State& s, JS::HandleObject animation, double rate);
bool UpdatePlaybackRate(State& s, JS::HandleObject animation, double rate);
bool UpdateFinished(State& s, JS::HandleObject animation, bool didSeek, bool sync);
bool AnimationsFor(State& s, JS::HandleObject root, bool subtree, JS::MutableHandleValue result);
bool InstallAnimations(State& s);
bool InstallElementMembers(State& s, JS::HandleObject elementPrototype);
}  // namespace ruyijs::web_animation
#endif
