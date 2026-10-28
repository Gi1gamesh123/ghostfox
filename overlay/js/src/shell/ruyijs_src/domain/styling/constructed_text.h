// Constructed stylesheet replacement discards top-level @import statements.
// The existing StyleSheetAggregate remains the only rule grammar.
#pragma once
#include <string>

namespace ruyijs::domain::styling {
inline std::string WithoutImports(const std::string& text) {
  std::string out; char quote = 0; bool escaped = false; int depth = 0;
  for (size_t i = 0; i < text.size();) {
    char c = text[i];
    if (!quote && !depth && c == '@') {
      size_t at = i + 1; std::string word;
      while (at < text.size() && ((text[at] >= 'a' && text[at] <= 'z') ||
                                 (text[at] >= 'A' && text[at] <= 'Z'))) {
        char ch = text[at++]; word += ch >= 'A' && ch <= 'Z' ? char(ch + 32) : ch;
      }
      if (word == "import") {
        char q = 0; bool escape = false;
        for (i = at; i < text.size(); ++i) {
          char ch = text[i];
          if (escape) { escape = false; continue; }
          if (ch == '\\') { escape = true; continue; }
          if (q) { if (ch == q) q = 0; continue; }
          if (ch == '\'' || ch == '"') { q = ch; continue; }
          if (ch == ';') { ++i; break; }
        }
        continue;
      }
    }
    out += c; ++i;
    if (escaped) { escaped = false; continue; }
    if (c == '\\') { escaped = true; continue; }
    if (quote) { if (c == quote) quote = 0; continue; }
    if (c == '\'' || c == '"') quote = c;
    else if (c == '{') ++depth;
    else if (c == '}') --depth;
  }
  return out;
}
}  // namespace ruyijs::domain::styling
