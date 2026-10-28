#pragma once
#include "document_model/model.h"
#include "js/RootingAPI.h"
namespace ruyijs::document_model {
class Realm;
struct FormField { DomString name, value; bool emptyFile = false; };
struct FormSubmission { DomString action, method, encoding, target; uint64_t version = 0; };
using FormDataPort = bool (*)(void*, Realm&, const std::vector<FormField>&, JS::MutableHandleObject);
using FormNavigatePort = bool (*)(void*, Realm&, JS::HandleObject form,
                                  JS::HandleObject data, const FormSubmission&);
struct Wrapper;
bool SubmitForm(Wrapper&, JS::HandleValue submitter, bool request);
bool ActivateFormControl(Wrapper&);
bool ResetForm(Wrapper&);
bool ControlForm(Wrapper&, JS::MutableHandleValue);
bool ValidateForm(Wrapper&, bool* valid);
}
