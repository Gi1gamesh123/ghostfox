#include "domain/styling/external_style_value.h"
#include <cctype>
namespace ruyijs::domain::styling {
namespace {
bool Space(char c) { return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\f'; }
std::string Lower(std::string value) {
  for (char& c : value) if (c>='A' && c<='Z') c += 'a'-'A';
  return value;
}
std::string Trim(std::string value) {
  while (!value.empty() && Space(value.back())) value.pop_back();
  const auto first = value.find_first_not_of(" \t\r\n\f");
  return first==std::string::npos ? std::string() : value.substr(first);
}
}
bool StyleRelation(const std::string& rel, const char* token) {
  size_t i=0;
  while (i<rel.size()) {
    while (i<rel.size() && Space(rel[i])) ++i;
    const size_t start=i;
    while (i<rel.size() && !Space(rel[i])) ++i;
    if (Lower(rel.substr(start,i-start))==token) return true;
  }
  return false;
}
bool StyleType(const std::string& type) { return type.empty() || Lower(Trim(type))=="text/css"; }
bool CssResponseMime(const std::string& type) {
  return Lower(Trim(type.substr(0,type.find(';'))))=="text/css";
}
}
