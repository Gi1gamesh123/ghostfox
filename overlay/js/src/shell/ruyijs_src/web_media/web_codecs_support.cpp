// WebCodecs static isConfigSupported and the named DOMException the codec
// members throw or reject with. Support follows Firefox 158 on desktop Windows:
// the codec families its platform/software decoders and encoders accept.
#include "web_media/web_codecs.h"
#include "runtime/runtime.h"
#include "jsapi.h"
#include "js/CallAndConstruct.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include <string>

namespace ruyijs::web_media {
namespace {
bool Prefix(const std::string& text, const char* prefix) { return text.rfind(prefix, 0) == 0; }
bool Supported(CodecKind kind, const std::string& codec) {
  switch (kind) {
    case VideoDecode:
      return codec == "vp8" || Prefix(codec, "vp09.") || Prefix(codec, "avc1.") || Prefix(codec, "av01.");
    case VideoEncode:
      return codec == "vp8" || Prefix(codec, "vp09.") || Prefix(codec, "avc1.");
    case AudioDecode:
      return codec == "opus" || codec == "vorbis" || codec == "flac" || codec == "mp3" ||
             codec == "mp4a.40.2" || codec == "mp4a.40.02" || codec == "mp4a.67" || Prefix(codec, "pcm-");
    case AudioEncode:
      return codec == "opus";
  }
  return false;
}
}  // namespace

bool CodecError(State& state, const char* name, const std::string& message, JS::MutableHandleValue out) {
  JSContext* cx = state.cx;
  JS::RootedString label(cx, JS_NewStringCopyZ(cx, name));
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, message.c_str()));
  if (!label || !text) return false;
  JS::RootedValueArray<2> values(cx);
  values[0].setString(text);
  values[1].setString(label);
  JS::RootedObject error(cx);
  if (!JS::Construct(cx, state.exceptionConstructor, values, &error)) return false;
  out.setObject(*error);
  return true;
}

bool CodecIsConfigSupported(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const CodecKind kind = CodecKind(WebKit::Slot(args));
  JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  JS::RootedObject result(cx, JS_NewPlainObject(cx)), echoed(cx, JS_NewPlainObject(cx));
  if (!promise || !result || !echoed) return false;
  JS::RootedObject config(cx, args.get(0).isObject() ? &args.get(0).toObject() : nullptr);
  JS::RootedValue codec(cx), sampleRate(cx), channels(cx), width(cx), height(cx);
  auto read = [&](const char* field, JS::MutableHandleValue value) {
    value.setUndefined();
    return !config || JS_GetProperty(cx, config, field, value);
  };
  if (!read("codec", &codec) || !read("sampleRate", &sampleRate) ||
      !read("numberOfChannels", &channels) || !read("width", &width) || !read("height", &height)) return false;
  std::string codecString;
  if (codec.isString()) {
    JS::RootedString codecText(cx, codec.toString());
    if (!ToUTF8(cx, codecText, &codecString)) return false;
  }
  auto define = [&](const char* name, JS::HandleValue value) {
    return JS_DefineProperty(cx, echoed, name, value, JSPROP_ENUMERATE);
  };
  auto text = [&](const char* name, const char* value) {
    JS::RootedString s(cx, JS_NewStringCopyZ(cx, value));
    JS::RootedValue v(cx, s ? JS::StringValue(s) : JS::UndefinedValue());
    return s && define(name, v);
  };
  // Firefox echoes the config with its dictionary defaults, members in IDL (alphabetical) order.
  bool ok = true;
  if (kind == AudioDecode) {
    ok = define("codec", codec) && define("numberOfChannels", channels) && define("sampleRate", sampleRate);
  } else if (kind == AudioEncode) {
    ok = text("bitrateMode", "variable") && define("codec", codec) &&
         define("numberOfChannels", channels) && define("sampleRate", sampleRate);
  } else if (kind == VideoDecode) {
    ok = define("codec", codec) && text("hardwareAcceleration", "no-preference");
  } else {
    ok = text("alpha", "discard") && text("bitrateMode", "variable") && define("codec", codec) &&
         text("hardwareAcceleration", "no-preference") && define("height", height) &&
         text("latencyMode", "quality") && define("width", width);
  }
  if (!ok) return false;
  JS::RootedValue echoedValue(cx, JS::ObjectValue(*echoed));
  JS::RootedValue supported(cx, JS::BooleanValue(Supported(kind, codecString)));
  JS::RootedValue resultValue(cx, JS::ObjectValue(*result));
  if (!JS_DefineProperty(cx, result, "config", echoedValue, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, result, "supported", supported, JSPROP_ENUMERATE) ||
      !JS::ResolvePromise(cx, promise, resultValue)) return false;
  args.rval().setObject(*promise);
  return true;
}
}  // namespace ruyijs::web_media
