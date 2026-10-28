// EffectTiming conversion and validation, getTiming/getComputedTiming and
// updateTiming (docs/owners/WEB_ANIMATION.md). Conversion follows WebIDL member
// order; validation follows Firefox TimingParams (duration, iterationStart,
// iterations, easing) and its error text.
#include "web_animation/state.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include <cmath>
#include <cstdio>

namespace ruyijs::web_animation {
namespace {
// Firefox formats these numbers with printf %g ("nan", "inf", "-1").
std::string G(double value) {
  char buffer[32];
  std::snprintf(buffer, sizeof buffer, "%g", value);
  return buffer;
}
bool Enum(JSContext* cx, JS::HandleValue value, const char* const* names, int count,
          const std::string& prefix, const char* member, const char* dictionary,
          const char* enumeration, int* out) {
  std::string text;
  if (!Text(cx, value, &text)) return false;
  for (int i = 0; i < count; ++i) if (text == names[i]) { *out = i; return true; }
  return TypeFail(cx, prefix + "'" + text + "' (value of '" + member + "' member of " + dictionary +
                      ") is not a valid value for enumeration " + enumeration + ".");
}
bool Finite(JSContext* cx, JS::HandleValue value, const std::string& prefix, const char* member,
            const char* dictionary, double* out) {
  if (!JS::ToNumber(cx, value, out)) return false;
  if (std::isfinite(*out)) return true;
  return TypeFail(cx, prefix + "'" + member + "' member of " + dictionary +
                      " is not a finite floating-point value.");
}
}  // namespace

bool ParseTiming(JSContext* cx, JS::HandleValue input, bool partial, const char* dictionary,
                 const std::string& prefix, model::EffectTiming* t) {
  if (input.isNullOrUndefined()) return true;
  JS::RootedValue duration(cx), easing(cx);
  std::optional<double> iterationStart, iterations;
  if (input.isObject()) {
    JS::RootedObject object(cx, &input.toObject());
    JS::RootedValue v(cx);
    int index = 0;
    // EffectTiming members, lexicographic: conversion first, validation after.
    if (!JS_GetProperty(cx, object, "delay", &v)) return false;
    if (!v.isUndefined() && !Finite(cx, v, prefix, "delay", dictionary, &t->delay)) return false;
    if (!JS_GetProperty(cx, object, "direction", &v)) return false;
    if (!v.isUndefined()) {
      if (!Enum(cx, v, model::kDirectionNames, 4, prefix, "direction", dictionary, "PlaybackDirection", &index))
        return false;
      t->direction = model::Direction(index);
    }
    if (!JS_GetProperty(cx, object, "duration", &duration)) return false;
    if (!JS_GetProperty(cx, object, "easing", &easing)) return false;
    if (!JS_GetProperty(cx, object, "endDelay", &v)) return false;
    if (!v.isUndefined() && !Finite(cx, v, prefix, "endDelay", dictionary, &t->endDelay)) return false;
    if (!JS_GetProperty(cx, object, "fill", &v)) return false;
    if (!v.isUndefined()) {
      if (!Enum(cx, v, model::kFillNames, 5, prefix, "fill", dictionary, "FillMode", &index)) return false;
      t->fill = model::Fill(index);
    }
    if (!JS_GetProperty(cx, object, "iterationStart", &v)) return false;
    double number = 0;
    if (!v.isUndefined()) {
      if (!Finite(cx, v, prefix, "iterationStart", dictionary, &number)) return false;
      iterationStart = number;
    }
    if (!JS_GetProperty(cx, object, "iterations", &v)) return false;
    if (!v.isUndefined()) {
      if (!JS::ToNumber(cx, v, &number)) return false;
      iterations = number;
    }
  } else if (!partial) {
    // (unrestricted double or KeyframeEffectOptions): anything else is the duration number.
    double ms = 0;
    if (!JS::ToNumber(cx, input, &ms)) return false;
    if (!(ms >= 0)) return TypeFail(cx, prefix + "Duration value " + G(ms) + " is less than 0");
    t->durationAuto = false;
    t->duration = ms;
  } else {
    return TypeFail(cx, prefix + "Argument 1 can't be converted to a dictionary.");
  }
  if (!duration.isUndefined()) {
    if (duration.isNumber()) {  // (unrestricted double or DOMString): only a Number is the double
      const double ms = duration.toNumber();
      if (!(ms >= 0)) return TypeFail(cx, prefix + "Duration (" + G(ms) + ") must be nonnegative");
      t->durationAuto = false;
      t->duration = ms;
    } else {
      std::string text;
      if (!Text(cx, duration, &text)) return false;
      if (text != "auto") return TypeFail(cx, prefix + "Invalid duration '" + text + "'.");
      t->durationAuto = true;
      t->duration = 0;
    }
  }
  if (iterationStart) {
    if (*iterationStart < 0)
      return TypeFail(cx, prefix + "Iteration start (" + G(*iterationStart) + ") must not be negative");
    t->iterationStart = *iterationStart;
  }
  if (iterations) {
    if (std::isnan(*iterations)) return TypeFail(cx, prefix + "Iterations must not be NaN");
    if (*iterations < 0)
      return TypeFail(cx, prefix + "Iterations (" + G(*iterations) + ") must not be negative");
    t->iterations = *iterations;
  }
  if (!easing.isUndefined()) {
    std::string text;
    if (!Text(cx, easing, &text)) return false;
    if (!model::ParseEasing(text, &t->easing)) return TypeFail(cx, prefix + "Invalid easing '" + text + "'.");
  }
  return true;
}

bool StoreTiming(JSObject* e, const model::EffectTiming& t) {
  State& s = OwnerOf(e);
  JS::RootedValue easing(s.cx);
  if (!NewString(s.cx, t.easing.text, &easing)) return false;
  JS::SetReservedSlot(e, kEEasing, easing);
  JS::SetReservedSlot(e, kEDelay, JS::DoubleValue(t.delay));
  JS::SetReservedSlot(e, kEEndDelay, JS::DoubleValue(t.endDelay));
  JS::SetReservedSlot(e, kEIterStart, JS::DoubleValue(t.iterationStart));
  JS::SetReservedSlot(e, kEIterations, JS::DoubleValue(t.iterations));
  JS::SetReservedSlot(e, kEDuration, t.durationAuto ? JS::UndefinedValue() : JS::DoubleValue(t.duration));
  JS::SetReservedSlot(e, kEFill, JS::Int32Value(int32_t(t.fill)));
  JS::SetReservedSlot(e, kEDirection, JS::Int32Value(int32_t(t.direction)));
  return true;  // The serialized easing is the stored form; ReadTiming re-parses it.
}

model::EffectTiming ReadTiming(JSObject* e) {
  model::EffectTiming t;
  t.delay = JS::GetReservedSlot(e, kEDelay).toDouble();
  t.endDelay = JS::GetReservedSlot(e, kEEndDelay).toDouble();
  t.iterationStart = JS::GetReservedSlot(e, kEIterStart).toDouble();
  t.iterations = JS::GetReservedSlot(e, kEIterations).toDouble();
  const JS::Value duration = JS::GetReservedSlot(e, kEDuration);
  t.durationAuto = duration.isUndefined();
  t.duration = t.durationAuto ? 0 : duration.toDouble();
  t.fill = model::Fill(JS::GetReservedSlot(e, kEFill).toInt32());
  t.direction = model::Direction(JS::GetReservedSlot(e, kEDirection).toInt32());
  const JS::Value easing = JS::GetReservedSlot(e, kEEasing);
  if (easing.isString()) {
    State& s = OwnerOf(e);
    JS::RootedValue value(s.cx, easing);
    std::string text;
    if (Text(s.cx, value, &text)) model::ParseEasing(text, &t.easing);
    else JS_ClearPendingException(s.cx);
  }
  return t;
}

bool TimingObject(JSContext* cx, const model::EffectTiming& t, bool computed, JSObject* effect,
                  JS::MutableHandleValue result) {
  JS::RootedObject o(cx, JS_NewPlainObject(cx));
  JS::RootedValue v(cx);
  if (!o) return false;
  auto put = [&](const char* name) { return JS_DefineProperty(cx, o, name, v, JSPROP_ENUMERATE); };
  auto num = [&](const char* name, double d) { v.setNumber(d); return put(name); };
  auto opt = [&](const char* name, const std::optional<double>& d) {
    if (d) v.setNumber(*d); else v.setNull();
    return put(name);
  };
  model::ComputedTiming c;
  if (computed) {
    JSObject* animation = effect ? JS::GetReservedSlot(effect, kEAnimation).toObjectOrNull() : nullptr;
    c = animation ? AnimationTiming(animation) : model::Compute(t, std::nullopt, false);
  }
  const model::Fill fill = computed ? c.fill : t.fill;
  if (!num("delay", t.delay) || !NewString(cx, model::kDirectionNames[int(t.direction)], &v) ||
      !put("direction")) return false;
  if (t.durationAuto && !computed) { if (!NewString(cx, "auto", &v)) return false; }
  else v.setNumber(t.IterationDuration());
  if (!put("duration") || !NewString(cx, t.easing.text, &v) || !put("easing") ||
      !num("endDelay", t.endDelay) || !NewString(cx, model::kFillNames[int(fill)], &v) ||
      !put("fill") || !num("iterationStart", t.iterationStart) || !num("iterations", t.iterations))
    return false;
  if (computed && (!num("activeDuration", t.ActiveDuration()) || !opt("currentIteration", c.currentIteration) ||
                   !num("endTime", t.EndTime()) || !opt("localTime", c.localTime) ||
                   !opt("progress", c.progress) || !num("startTime", 0)))
    return false;
  result.setObject(*o);
  return true;
}
}  // namespace ruyijs::web_animation
