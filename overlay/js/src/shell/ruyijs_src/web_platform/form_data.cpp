// FormData. One ordered array of [name, value] pairs lives in the object's slot and
// every method reads and writes that array, so the pairs the iterator yields and
// the pairs a future body serializer reads are the same. A value is a string or a
// File; a Blob passed to append/set is materialized as a File with the spec's
// default name, never a second byte store.
#include "web_platform/state.h"
#include "web_platform/form_submission.h"
#include "web_platform/form_data_internal.h"

#include "runtime/native_constructor_trace.h"
#include "runtime/web_kit.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include <string>
#include <vector>

namespace ruyijs::web_platform {
const JSClass kFormDataClass = {"FormData", JSCLASS_HAS_RESERVED_SLOTS(kFormDataSlotCount)};

bool FormDataBrand(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out) {
  return WebKit::Check(cx, self, &kFormDataClass, out,
                       "FormData member called on an incompatible receiver");
}

bool FormDataList(JSContext* cx, JS::HandleObject self, JS::MutableHandleObject list) {
  JS::Value stored = JS::GetReservedSlot(self, kFormDataEntriesSlot);
  if (!stored.isObject()) return WebKit::Fail(cx, "FormData is not initialized");
  list.set(&stored.toObject());
  return true;
}

// The Blob-to-File materialization: the bytes and type come from the one Blob
// store and the name from the filename argument (or the File's own name).
bool FormDataValue(State& state, JS::HandleValue value, JS::HandleValue filename,
                   JS::MutableHandleValue out) {
  JSContext* cx = state.cx;
  if (!IsBlob(value)) {
    JS::RootedString text(cx, JS::ToString(cx, value));
    if (!text) return false;
    out.setString(text);
    return true;
  }
  JS::RootedObject blob(cx, &value.toObject());
  std::string name;
  if (!filename.isUndefined()) {
    if (!Convert(cx, filename, &name)) return false;
  } else if (JS::GetClass(blob) == &kFileClass) {
    JS::RootedValue stored(cx, JS::GetReservedSlot(blob, kFileSlotName));
    if (!Convert(cx, stored, &name)) return false;
  } else {
    name = "blob";
  }
  JS::RootedValue type_value(cx, JS::GetReservedSlot(blob, kBlobTypeSlot));
  std::string type;
  if (!Convert(cx, type_value, &type)) return false;
  const uint8_t* data = nullptr;
  size_t length = 0;
  if (!BlobBytes(cx, blob, &data, &length)) return false;
  const std::string bytes(reinterpret_cast<const char*>(data), length);
  JS::RootedObject file(
      cx, MakeFile(state, bytes, type, name, state.file.prototype));
  if (!file) return false;
  out.setObject(*file);
  return true;
}

bool MakeFormData(State& state, JS::HandleObject prototype, JS::MutableHandleObject out) {
  JSContext* cx = state.cx;
  JS::RootedObject object(cx, state.kit.New(&kFormDataClass, prototype));
  JS::RootedObject entries(cx, JS::NewArrayObject(cx, 0));
  if (!object || !entries) return false;
  JS::SetReservedSlot(object, kFormDataEntriesSlot, JS::ObjectValue(*entries));
  out.set(object);
  return true;
}
bool PopulateFormData(State& state, JS::HandleObject form,
                      const std::vector<FormField>& fields) {
  JSContext* cx = state.cx;
  JS::RootedObject list(cx);
  if (!FormDataList(cx, form, &list)) return false;
  uint32_t index = 0;
  for (const auto& field : fields) {
    JS::RootedString name(cx, JS_NewUCStringCopyN(cx, field.name.data(), field.name.size()));
    JS::RootedString text(cx, JS_NewUCStringCopyN(cx, field.value.data(), field.value.size()));
    if (!name || !text) return false;
    JS::RootedValue key(cx, JS::StringValue(name)), value(cx, JS::StringValue(text));
    JS::RootedObject pair(cx);
    if (field.emptyFile) {
      JS::RootedObject file(cx, MakeFile(state, "", "application/octet-stream", "",
                                         state.file.prototype));
      if (!file) return false;
      value.setObject(*file);
    }
    if (!form_data_internal::NewPair(cx, key, value, &pair) ||
        !form_data_internal::Push(cx, list, index++, pair))
      return false;
  }
  return true;
}
bool FormDataConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  NativeConstructorTrace trace(cx, args);
  JS::RootedObject prototype(cx), object(cx);
  if (!state.kit.ConstructProto(args, state.formData.prototype, &prototype) ||
      !MakeFormData(state, prototype, &object)) return false;
  if (args.length() && !args[0].isNullOrUndefined()) {
    if (!args[0].isObject() || !state.formFieldsPort)
      return WebKit::Fail(cx, "FormData constructor requires an HTMLFormElement");
    JS::RootedObject inputForm(cx, &args[0].toObject());
    std::vector<FormField> fields;
    if (!state.formFieldsPort(state.formFieldsContext, inputForm,
                              args.length() > 1 ? args[1] : JS::UndefinedHandleValue,
                              &fields) || !PopulateFormData(state, object, fields))
      return false;
  } else if (args.length() > 1 && !args[1].isUndefined()) {
    return WebKit::Fail(cx, "FormData submitter requires a form");
  }
  args.rval().setObject(*object);
  return trace.Finish(true);
}
}  // namespace ruyijs::web_platform
