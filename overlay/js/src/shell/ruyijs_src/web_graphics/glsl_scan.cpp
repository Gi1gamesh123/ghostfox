// Declaration scanning of the shader subset: the global declarations outside the
// braces and the assignments inside main(). The expression evaluator is in
// glsl_eval.cpp; a construct neither understands makes the shader fail to draw.
#include "web_graphics/glsl.h"

namespace ruyijs::web_graphics {
namespace {
using glsl_detail::IsName;
using glsl_detail::IsSpace;
using glsl_detail::Trim;

bool KindOf(const std::string& word) {
  return word == "attribute" || word == "uniform" || word == "varying" ||
         word == "in" || word == "out";
}

// "<kind> <type> <name>" from one declaration section; an array keeps its name.
bool Declaration(const std::string& section, GlslDeclaration* out) {
  std::vector<std::string> words;
  std::string word;
  for (char c : section) {
    if (IsName(c) || c == '[' || c == ']') { word.push_back(c); continue; }
    if (!word.empty()) { words.push_back(word); word.clear(); }
  }
  if (!word.empty()) words.push_back(word);
  for (size_t index = 0; index + 2 < words.size(); ++index) {
    if (!KindOf(words[index])) continue;
    out->kind = words[index];
    out->type = words[index + 1];
    out->name = words[index + 2];
    const size_t bracket = out->name.find('[');
    if (bracket != std::string::npos) out->name.resize(bracket);
    return !out->name.empty();
  }
  return false;
}

// "target = expression" from one statement; anything without a top-level '=' is
// skipped (declarations, if/else bodies and control flow are not evaluated).
bool Assignment(const std::string& section, GlslStatement* out) {
  int depth = 0;
  for (size_t index = 0; index < section.size(); ++index) {
    const char c = section[index];
    if (c == '(') ++depth;
    if (c == ')') --depth;
    if (c != '=' || depth != 0) continue;
    if (index + 1 < section.size() && section[index + 1] == '=') return false;
    const std::string target = Trim(section.substr(0, index));
    if (target.empty() || !IsName(target[0])) return false;
    out->target = target;
    out->expression = Trim(section.substr(index + 1));
    return !out->expression.empty();
  }
  return false;
}
}  // namespace

bool GlslParse(const std::string& source, GlslStage* stage) {
  stage->source = source;
  const char* p = source.data();
  const char* end = p + source.size();
  int depth = 0;
  std::string section;
  while (p < end) {
    const char c = *p;
    // A space is a separator the declaration and statement readers keep, so a
    // section never runs its tokens together.
    if (IsSpace(c)) {
      if (!section.empty()) section.push_back(' ');
      ++p;
      continue;
    }
    if (c == '/' && p + 1 < end && (p[1] == '/' || p[1] == '*')) {
      const bool line = p[1] == '/';
      p += 2;
      while (p < end && (line ? *p != '\n' : !(p + 1 < end && p[0] == '*' && p[1] == '/'))) ++p;
      if (!line && p < end) p += 2;
      continue;
    }
    if (c == '#') {
      while (p < end && *p != '\n') ++p;
      continue;
    }
    if (c == '{') { ++depth; section.clear(); ++p; continue; }
    if (c == '}') { if (depth > 0) --depth; section.clear(); ++p; continue; }
    if (c == ';') {
      if (depth > 0) {
        GlslStatement statement;
        if (Assignment(section, &statement)) stage->statements.push_back(statement);
      } else {
        GlslDeclaration declaration;
        if (Declaration(section, &declaration)) stage->globals.push_back(declaration);
      }
      section.clear();
      ++p;
      continue;
    }
    section.push_back(c);
    ++p;
  }
  return !stage->statements.empty() || !stage->globals.empty();
}

std::vector<GlslDeclaration> GlslDeclarations(const GlslStage& stage, const char* kind) {
  std::vector<GlslDeclaration> out;
  for (const GlslDeclaration& declaration : stage.globals)
    if (declaration.kind == kind) out.push_back(declaration);
  return out;
}
}  // namespace ruyijs::web_graphics
