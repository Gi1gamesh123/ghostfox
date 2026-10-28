// The "webgl" environment-profile group's configure and describe ports. The
// profile only carries overrides; the effective values (Firefox's masked and
// unmasked GL strings plus the imported limits) come from model.cpp.
#include "web_graphics/internal.h"

#include "js/Array.h"
#include "js/PropertyAndElement.h"
#include "runtime/runtime.h"

namespace ruyijs::web_graphics {
namespace {
const char* const kFields[kOverrideCount] = {
    "vendor", "renderer", "version", "glslVersion", "unmaskedVendor",
    "unmaskedRenderer", "version2", "glslVersion2"};
const char* const kAttributeNames[kAttributeCount] = {
    "alpha", "antialias", "depth", "failIfMajorPerformanceCaveat",
    "powerPreference", "premultipliedAlpha", "preserveDrawingBuffer", "stencil"};

// One extension list: getSupportedExtensions() answers exactly these names, in
// this order, and getExtension() only recognises a name in the list.
bool Extensions(JSContext* cx, JS::HandleObject group, const char* member, bool second,
                Model& model) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, group, member, &value)) return false;
  if (value.isUndefined()) return true;
  bool array = false;
  if (!value.isObject() || !JS::IsArrayObject(cx, value, &array) || !array)
    return WebKit::Fail(cx, "webgl extension list must be an array of names");
  JS::RootedObject list(cx, &value.toObject());
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, list, &length)) return false;
  std::vector<std::string> names;
  for (uint32_t index = 0; index < length; ++index) {
    JS::RootedValue item(cx);
    std::string name;
    if (!JS_GetElement(cx, list, index, &item) || !item.isString()) return false;
    JS::RootedString text(cx, item.toString());
    if (!ToUTF8(cx, text, &name)) return false;
    names.push_back(std::move(name));
  }
  return model.OverrideExtensions(second, names) || (JS_ReportOutOfMemory(cx), false);
}

// "webgl.limits": pname -> value. The schema validated the value type; this
// rejects a pname that is not a numeric state query of either context version.
bool Limits(JSContext* cx, JS::HandleObject group, Model& model) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, group, "limits", &value)) return false;
  if (value.isUndefined()) return true;
  if (!value.isObject()) return WebKit::Fail(cx, "webgl.limits must be an object");
  JS::RootedObject table(cx, &value.toObject());
  JS::Rooted<JS::IdVector> keys(cx, JS::IdVector(cx));
  if (!JS_Enumerate(cx, table, &keys)) return false;
  for (size_t key = 0; key < keys.length(); ++key) {
    JS::RootedValue name(cx), answer(cx);
    JS::RootedString text(cx);
    std::string pname;
    if (!JS_IdToValue(cx, keys[key], &name) || !(text = JS::ToString(cx, name)) ||
        !ToUTF8(cx, text, &pname) || !JS_GetPropertyById(cx, table, keys[key], &answer) ||
        !answer.isNumber())
      return WebKit::Fail(cx, "webgl.limits must map a pname to a number");
    if (!model.OverrideLimit(pname, answer.toNumber()))
      return WebKit::Fail(cx, "webgl.limits only overrides numeric parameter pnames");
  }
  return true;
}

// The getContextAttributes() dictionary the profile pins. The schema already
// checked the members' types and the powerPreference allow-list; this only
// hands them to the one model that answers getParameter and the attribute
// dictionary, so no value is duplicated in the adapter.
bool ContextAttributes(JSContext* cx, JS::HandleValue value, Model& model) {
  if (!value.isObject())
    return WebKit::Fail(cx, "webgl.attributes must be an object");
  JS::RootedObject object(cx, &value.toObject());
  for (int32_t index = 0; index < kAttributeCount; ++index) {
    JS::RootedValue field(cx);
    if (!JS_GetProperty(cx, object, kAttributeNames[index], &field)) return false;
    if (field.isUndefined()) continue;
    if (index == kPowerPreference) {
      JS::RootedString text(cx, JS::ToString(cx, field));
      std::string utf8;
      if (!text || !ToUTF8(cx, text, &utf8)) return false;
      if (!model.OverridePowerPreference(utf8))
        return WebKit::Fail(cx, "webgl.attributes.powerPreference is not an allowed value");
      continue;
    }
    if (!field.isBoolean())
      return WebKit::Fail(cx, "webgl.attributes members must be boolean");
    if (!model.OverrideAttribute(index, field.toBoolean())) {
      JS_ReportOutOfMemory(cx);
      return false;
    }
  }
  return true;
}
}  // namespace

bool GraphicsPort(void* context, JS::HandleObject group) {
  State& state = *static_cast<State*>(context);
  JSContext* cx = state.cx;
  for (int32_t index = 0; index < kOverrideCount; ++index) {
    JS::RootedValue value(cx);
    if (!JS_GetProperty(cx, group, kFields[index], &value)) return false;
    if (value.isUndefined()) continue;
    JS::RootedString text(cx, JS::ToString(cx, value));
    std::string utf8;
    if (!text || !ToUTF8(cx, text, &utf8)) return false;
    if (!state.model.Override(index, utf8)) {
      JS_ReportOutOfMemory(cx);
      return false;
    }
  }
  JS::RootedValue attributes(cx);
  if (!JS_GetProperty(cx, group, "attributes", &attributes)) return false;
  if (!attributes.isUndefined() && !ContextAttributes(cx, attributes, state.model))
    return false;
  if (!Extensions(cx, group, "extensions", false, state.model) ||
      !Extensions(cx, group, "extensions2", true, state.model))
    return false;
  return Limits(cx, group, state.model);
}

bool GraphicsDescribe(void* context, JS::MutableHandleValue out) {
  State& state = *static_cast<State*>(context);
  JSContext* cx = state.cx;
  const GlValues& values = state.model.values();
  const std::string* texts[kOverrideCount] = {
      &values.vendor, &values.renderer, &values.version,
      &values.shading_language_version, &values.unmasked_vendor,
      &values.unmasked_renderer, &values.version2,
      &values.shading_language_version2};
  JS::RootedObject group(cx, JS_NewPlainObject(cx));
  if (!group) return false;
  for (int32_t index = 0; index < kOverrideCount; ++index) {
    JS::RootedString text(cx,
        JS_NewStringCopyN(cx, texts[index]->data(), texts[index]->size()));
    if (!text || !JS_DefineProperty(cx, group, kFields[index], text, JSPROP_ENUMERATE))
      return false;
  }
  // The effective context attributes, in the dictionary's own member order.
  const web_graphics::Attributes& attributes = values.attributes;
  const bool members[] = {attributes.alpha, attributes.antialias, attributes.depth,
                          attributes.fail_if_major_performance_caveat};
  const bool tail[] = {attributes.premultiplied_alpha,
                       attributes.preserve_drawing_buffer, attributes.stencil};
  JS::RootedObject dictionary(cx, JS_NewPlainObject(cx));
  if (!dictionary) return false;
  for (int32_t index = 0; index < kAttributeCount; ++index) {
    JS::RootedValue field(cx);
    if (index == kPowerPreference) {
      JS::RootedString text(cx, JS_NewStringCopyN(cx,
          attributes.power_preference.data(), attributes.power_preference.size()));
      if (!text) return false;
      field.setString(text);
    } else {
      field.setBoolean(index < 4 ? members[index] : tail[index - 5]);
    }
    if (!JS_DefineProperty(cx, dictionary, kAttributeNames[index], field,
                           JSPROP_ENUMERATE)) return false;
  }
  if (!JS_DefineProperty(cx, group, "attributes", dictionary, JSPROP_ENUMERATE))
    return false;
  // The extension lists are data, not derived values: report them verbatim.
  const std::vector<std::string>* lists[2] = {&values.supported_extensions,
                                              &values.supported_extensions2};
  const char* const names[2] = {"extensions", "extensions2"};
  for (int32_t index = 0; index < 2; ++index) {
    JS::RootedObject list(cx, JS::NewArrayObject(cx, lists[index]->size()));
    if (!list) return false;
    for (size_t item = 0; item < lists[index]->size(); ++item) {
      const std::string& name = (*lists[index])[item];
      JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(
          name.data(), name.size())));
      if (!text || !JS_DefineElement(cx, list, item, text, JSPROP_ENUMERATE))
        return false;
    }
    if (!JS_DefineProperty(cx, group, names[index], list, JSPROP_ENUMERATE))
      return false;
  }
  // The configured pname overrides, keyed the way a profile keys them.
  JS::RootedObject limits(cx, JS_NewPlainObject(cx));
  if (!limits) return false;
  for (const auto& entry : state.model.limits()) {
    const std::string key = std::to_string(entry.first);
    JS::RootedValue answer(cx, JS::NumberValue(entry.second));
    if (!JS_DefineProperty(cx, limits, key.c_str(), answer, JSPROP_ENUMERATE))
      return false;
  }
  if (!JS_DefineProperty(cx, group, "limits", limits, JSPROP_ENUMERATE))
    return false;
  out.setObject(*group);
  return true;
}
}  // namespace ruyijs::web_graphics
