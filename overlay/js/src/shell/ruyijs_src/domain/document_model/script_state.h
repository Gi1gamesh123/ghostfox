#pragma once
#include <cstdint>
#include <string_view>
namespace ruyijs::domain::document_model {
struct ScriptState {
  bool started = false;
  bool parser_inserted = false;
  bool force_async = true;
  // Document position (1-based line, UTF-16 column) of the script text the
  // parser created, as Firefox's nsScriptElement line/column numbers; 0 when
  // the text did not come from the document's markup.
  uint32_t line = 0, column = 0;
};
// HTML JavaScript MIME type essence matching, shared by preparation and loading.
template<class Char> bool script_classic(std::basic_string_view<Char> value) {
  auto space=[](Char c){return c==9||c==10||c==12||c==13||c==32;};
  while(!value.empty()&&space(value.front()))value.remove_prefix(1);
  while(!value.empty()&&space(value.back()))value.remove_suffix(1);
  for(std::string_view type : {"", "application/ecmascript", "application/javascript",
      "application/x-ecmascript", "application/x-javascript", "text/ecmascript",
      "text/javascript", "text/javascript1.0", "text/javascript1.1", "text/javascript1.2",
      "text/javascript1.3", "text/javascript1.4", "text/javascript1.5", "text/jscript",
      "text/livescript", "text/x-ecmascript", "text/x-javascript"}) {
    if(type.size()!=value.size())continue;
    bool same=true;
    for(size_t i=0;i<value.size();++i) {
      auto c=value[i];if(c>='A'&&c<='Z')c+=32;
      if(c!=Char(type[i])){same=false;break;}
    }
    if(same)return true;
  }
  return false;
}
}
