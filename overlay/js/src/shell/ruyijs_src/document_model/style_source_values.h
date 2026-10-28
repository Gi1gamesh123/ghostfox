// Shared original DOM source encoding; no stylesheet state or CSS parser.
#pragma once
#include "document_model/model.h"
#include "document_model/style_source.h"
namespace ruyijs::document_model {
struct State;
std::string StyleSourceText(DomStringView);
// UTF-8 to DOM UTF-16 (invalid sequences become U+FFFD).
DomString StyleSourceWide(const std::string&);
bool StyleSourceSpace(char);
std::string StyleSourceTitle(const std::string&);
bool ReadLinkStyleSource(State&, NodeKey, NodeKey, const NodeSnapshot&, StyleSheetSource*);
// The URL of the document that owns `node` (the base of its link hrefs).
bool StyleDocumentURL(State&, NodeKey node, std::string* out);
bool NotifyLinkStyle(State&, NodeKey);
}
