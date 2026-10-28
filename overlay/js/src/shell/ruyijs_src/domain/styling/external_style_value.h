// Authored CSS resource policy. No DOM, JS, transport or runtime dependency.
#pragma once
#include <string>
namespace ruyijs::domain::styling {
bool StyleRelation(const std::string& rel, const char* token);
bool StyleType(const std::string& type);
bool CssResponseMime(const std::string& type);
}
