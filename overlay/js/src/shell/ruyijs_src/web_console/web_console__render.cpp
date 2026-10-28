#include "web_console/render.h"
#include "web_console/internal.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Object.h"
#include "js/Wrapper.h"
#include "js/Symbol.h"
#include "js/Conversions.h"
#include "js/SavedFrameAPI.h"
#include "js/Stack.h"
#include "js/String.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace ruyijs::web_console {
namespace {
// U+2018/U+2019/U+201C/U+201D as explicit bytes: the donor frontend wraps labels
// in typographic quotes and these sources stay ASCII.
constexpr char kQuoteOpen[] = "\xE2\x80\x9C";
constexpr char kQuoteClose[] = "\xE2\x80\x9D";
constexpr char kApostrophe[] = "\xE2\x80\x99";

std::string Utf8(JSContext* cx, JS::HandleString string, size_t limit) {
  if (!string) return "";
  JS::UniqueChars bytes = JS_EncodeStringToUTF8(cx, string);
  if (!bytes) { JS_ClearPendingException(cx); return ""; }
  size_t length = std::min(std::strlen(bytes.get()), limit);
  while (length && (static_cast<unsigned char>(bytes.get()[length]) & 0xc0) == 0x80)
    --length;
  return std::string(bytes.get(), length);
}

std::string Shown(const std::string& label) {
  return label.empty() ? std::string("<no label>") : label;
}

// `Math.round(duration * 100) / 100` as the frontend prints it, without
// trailing zeros, so a whole millisecond renders as "50" and not "50.00".
std::string Fixed(double value) {
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%.2f", value);
  std::string text(buffer);
  if (text.find('.') != std::string::npos) {
    text.erase(text.find_last_not_of('0') + 1);
    if (!text.empty() && text.back() == '.') text.pop_back();
  }
  return text;
}

std::string Quoted(const std::string& label) {
  return std::string(kQuoteOpen) + label + kQuoteClose;
}
}

// A logged argument is kept, not converted: Firefox runs no page code for
// console.log arguments while no console consumer is attached, so an object's
// toString/valueOf/Symbol.toPrimitive/getters must not run here (DataDome's
// devtools probe logs an object with an overridden toString). Objects render as
// their engine class name, symbols as their description.
std::string Render(JSContext* cx, JS::HandleValue value) {
  if (value.isObject()) {
    JSObject* object = js::UncheckedUnwrap(&value.toObject());
    const char* name = JS::GetClass(object)->name;
    return std::string("[object ") + (name ? name : "Object") + "]";
  }
  if (value.isSymbol()) {
    JS::RootedSymbol symbol(cx, value.toSymbol());
    JS::RootedString description(cx, JS::GetSymbolDescription(symbol));
    return "Symbol(" + Utf8(cx, description, State::kLabelLimit) + ")";
  }
  return LabelText(cx, value);
}

std::string LabelText(JSContext* cx, JS::HandleValue value) {
  JS::RootedString text(cx, JS::ToString(cx, value));
  if (!text) { JS_ClearPendingException(cx); return "[unrenderable]"; }
  return Utf8(cx, text, State::kLabelLimit);
}

std::string Join(JSContext* cx, const JS::CallArgs& args, unsigned first) {
  std::string joined;
  for (unsigned index = first; index < args.length(); ++index) {
    if (index != first) joined += ' ';
    joined += Render(cx, args[index]);
    if (joined.size() >= Log::kTextLimit) break;
  }
  if (joined.size() > Log::kTextLimit) joined.resize(Log::kTextLimit);
  return joined;
}

std::string Label(JSContext* cx, const JS::CallArgs& args, const char* fallback) {
  return args.length() ? LabelText(cx, args[0]) : std::string(fallback);
}

std::string Counted(const std::string& label, int count) {
  return Shown(label) + ": " + std::to_string(count);
}

std::string CounterMissing(const std::string& label) {
  return "Counter " + Quoted(label) + " doesn" + kApostrophe + "t exist.";
}

std::string TimerExists(const std::string& label) {
  return "Timer " + Quoted(label) + " already exists.";
}

std::string TimerMissing(const std::string& label) {
  return "Timer " + Quoted(label) + " doesn" + kApostrophe + "t exist.";
}

std::string Timed(const std::string& label, double duration, bool ended) {
  std::string text = label + ": " + Fixed(duration) + "ms";
  return ended ? text + " - timer ended" : text;
}

std::vector<std::string> Capture(JSContext* cx) {
  JS::RootedObject stack(cx);
  if (!JS::CaptureCurrentStack(cx, &stack,
          JS::StackCapture(JS::MaxFrames(Log::kStackLimit + 1)))) {
    JS_ClearPendingException(cx);
    return {};
  }
  std::vector<std::string> frames;
  JS::RootedObject frame(cx, stack), parent(cx);
  JS::RootedString name(cx), source(cx);
  while (frame && frames.size() < Log::kStackLimit) {
    uint32_t line = 0;
    if (JS::GetSavedFrameSource(cx, nullptr, frame, &source) != JS::SavedFrameResult::Ok)
      break;
    JS::GetSavedFrameFunctionDisplayName(cx, nullptr, frame, &name);
    JS::GetSavedFrameLine(cx, nullptr, frame, &line);
    frames.push_back(Utf8(cx, name, 128) + "@" + Utf8(cx, source, 512) + ":" +
                     std::to_string(line));
    if (JS::GetSavedFrameParent(cx, nullptr, frame, &parent) != JS::SavedFrameResult::Ok)
      break;
    frame = parent;
  }
  JS_ClearPendingException(cx);
  return frames;
}
}
