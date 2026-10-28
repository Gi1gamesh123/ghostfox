#pragma once
#include "document_model/state.h"

namespace ruyijs::document_model {
// Original DOM wrapper and owner realm. Null name is only for trusted facades.
// |member| (e.g. "get textTracks") selects the donor binding's receiver
// message, "'<member>' called on an object that does not implement interface
// <interface_name>."; without it the shared control message is used.
// HTMLInputElement.files (html_input_files.cpp).
bool InputFiles(Wrapper& entry, JS::MutableHandleValue result);
bool SetInputFiles(Wrapper& entry, JS::HandleValue value);
bool ResetInputFiles(Wrapper& entry);
Wrapper* ControlReceiver(JSContext*, JS::HandleValue, const char* interface_name,
                         const char* member = nullptr);
bool ControlValidity(Wrapper&, JS::MutableHandleValue);
bool ControlForm(Wrapper&, JS::MutableHandleValue);
bool InstallControlValidity(State&);
bool ControlString(JSContext*, JS::HandleValue, DomString*);
bool StyleElementAccess(Wrapper&, StyleElementAction, bool, JS::MutableHandleValue);
}
