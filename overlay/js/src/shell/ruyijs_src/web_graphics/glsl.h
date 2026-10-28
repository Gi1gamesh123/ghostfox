#ifndef RUYIJS_WEB_GRAPHICS_GLSL_H
#define RUYIJS_WEB_GRAPHICS_GLSL_H
#include <cctype>
#include <cstddef>
#include <string>
#include <vector>

namespace ruyijs::web_graphics {
// One value of the shading-language subset this owner evaluates: a float, vec2,
// vec3 or vec4. Everything the draw probe's shaders need is built from these.
struct GlslValue {
  int count = 0;
  float value[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

// A global declaration: the kind word ("attribute", "uniform", "varying") its
// declared type ("vec2", ...) and its name.
struct GlslDeclaration {
  std::string kind, type, name;
};

// One assignment inside main(), e.g. "gl_Position" = "vec4(attrVertex,0,1)".
struct GlslStatement {
  std::string target, expression;
};

// One parsed shader stage: its globals and the assignments of its main().
struct GlslStage {
  std::vector<GlslDeclaration> globals;
  std::vector<GlslStatement> statements;
  // The source the stage was parsed from, kept so getShaderSource can answer it.
  std::string source;
};

namespace glsl_detail {
inline bool IsName(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_' ||
         std::isdigit(static_cast<unsigned char>(c));
}
inline bool IsSpace(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }
inline void SkipSpace(const char*& p, const char* end) {
  while (p < end && IsSpace(*p)) ++p;
}
inline std::string Trim(const std::string& text) {
  size_t first = 0, last = text.size();
  while (first < last && IsSpace(text[first])) ++first;
  while (last > first && IsSpace(text[last - 1])) --last;
  return text.substr(first, last - first);
}
}  // namespace glsl_detail

// Parses a vertex or fragment shader source. Declarations outside the braces and
// "<target> = <expression>;" assignments inside them are the whole language this
// evaluator understands; anything else is skipped, which keeps an unsupported
// shader from drawing rather than drawing wrongly.
bool GlslParse(const std::string& source, GlslStage* stage);
// The declarations of one kind, in source order.
std::vector<GlslDeclaration> GlslDeclarations(const GlslStage& stage, const char* kind);
// Resolves one identifier; returns null when the name is unknown to the stage.
using GlslLookup = const GlslValue* (*)(void* context, const char* name, size_t length);
bool GlslEvaluate(const std::string& expression, GlslLookup lookup, void* context,
                  GlslValue* out);
}  // namespace ruyijs::web_graphics
#endif
