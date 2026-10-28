#pragma once
#include <string>
#include <vector>
#include "js/RootingAPI.h"
namespace ruyijs::web_platform {
struct State;
struct FormField {
  std::u16string name, value;
  bool emptyFile = false;
};
// Runtime adapter for the Document Model's successful-controls owner. The
// WebPlatform realm owns FormData objects; the adapter only supplies the
// already-collected fields for a form in this realm.
using FormFieldsPort = bool (*)(void*, JS::HandleObject, JS::HandleValue,
                                std::vector<FormField>*);
bool MakeFormData(State&, JS::HandleObject prototype, JS::MutableHandleObject);
}
