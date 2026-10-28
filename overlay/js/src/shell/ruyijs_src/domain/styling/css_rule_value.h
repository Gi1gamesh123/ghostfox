// Canonical live CSSOM rule state. Native wrappers contain handles, not copies.
#pragma once
#include "domain/styling/media_list_value.h"
#include <memory>
#include <string>
#include <vector>
#include <variant>
#include "domain/styling/css_declarations.h"
namespace ruyijs::domain::styling {
struct CssomRuleValue {
  int kind=1;
  bool nested=false;
  std::string selector,condition;
  // One authority: normal rule text or ordered page descriptor rows.
  std::variant<std::string,std::vector<CssDeclarationValue>> declarations;
  std::string& StringDeclarations() {return std::get<std::string>(declarations);}
  const std::string& StringDeclarations() const {return std::get<std::string>(declarations);}
  std::vector<CssDeclarationValue>& PageDeclarations() {return std::get<std::vector<CssDeclarationValue>>(declarations);}
  const std::vector<CssDeclarationValue>& PageDeclarations() const {return std::get<std::vector<CssDeclarationValue>>(declarations);}
  std::string DeclarationText() const;
  // Present only for kind 8: exact f32 percentages determine find/delete
  // identity independently of the rounded CSSOM serialization.
  std::vector<float> key_positions;
  MediaListValue media;
  std::vector<std::shared_ptr<CssomRuleValue>> children;
  std::string ConditionText() const { return kind==4?media.text():condition; }
  std::string Text() const;
};
struct CssRuleParseResult {
  std::vector<std::shared_ptr<CssomRuleValue>> rules;
  bool valid=true;
};
CssRuleParseResult ParseCssRuleValues(const std::string&,bool strict=false);
}
