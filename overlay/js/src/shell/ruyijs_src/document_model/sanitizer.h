// The Document Model's Sanitizer adapter (Firefox 158 Sanitizer.webidl): WebIDL
// conversion into the configuration model (sanitizer_config.cpp) and the interface
// members (sanitizer_members.cpp). The model itself is domain-only.
#ifndef RUYIJS_DOCUMENT_MODEL_SANITIZER_H
#define RUYIJS_DOCUMENT_MODEL_SANITIZER_H
#include <string>
#include <vector>
#include "document_model/state.h"
#include "domain/document_model/sanitizer_config.h"

namespace ruyijs::document_model {
namespace sz = domain::document_model::sanitizer;
// Each conversion returns false only with an engine error pending; a WebIDL
// conversion failure leaves its TypeError text (prefixed by `where`) in *error.
bool SanitizerName(JSContext*, JS::HandleValue, const char* defaultNs, const char* dictNs,
                   const std::string& where, sz::Name*, std::string* error);
bool SanitizerElement(JSContext*, JS::HandleValue, const std::string& where, sz::Input::Element*, std::string* error);
bool SanitizerInput(JSContext*, JS::HandleObject dict, sz::Input*, std::string* error);
void SanitizerDefault(sz::Config*);
std::vector<std::string> SanitizerEventAttributes();
bool InstallSanitizer(State&);
bool NewSanitizer(State&, const JS::CallArgs&);
}  // namespace ruyijs::document_model
#endif
