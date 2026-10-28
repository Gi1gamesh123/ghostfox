// The expression evaluator of the shader subset: numbers, identifiers with an
// optional swizzle, type constructors, parentheses, and + - * / on values whose
// component counts match. Everything else fails the evaluation.
#include "web_graphics/glsl.h"

#include <cstdlib>

namespace ruyijs::web_graphics {
namespace {
using glsl_detail::IsName;
using glsl_detail::SkipSpace;

struct Parser {
  const char* p;
  const char* end;
  GlslLookup lookup;
  void* context;
  bool ok = true;

  void Skip() { SkipSpace(p, end); }
  bool Take(char c) {
    Skip();
    if (p < end && *p == c) { ++p; return true; }
    return false;
  }
  std::string Name() {
    Skip();
    const char* begin = p;
    while (p < end && IsName(*p)) ++p;
    return std::string(begin, p);
  }
  bool Number(float* out) {
    Skip();
    const char* begin = p;
    while (p < end && (std::isdigit(static_cast<unsigned char>(*p)) || *p == '.')) ++p;
    if (begin == p) return false;
    *out = std::strtof(std::string(begin, p).c_str(), nullptr);
    if (p < end && (*p == 'f' || *p == 'F')) ++p;
    return true;
  }
  static int ConstructorSize(const std::string& name) {
    if (name == "float" || name == "int" || name == "bool") return 1;
    if (name == "vec2") return 2;
    if (name == "vec3") return 3;
    if (name == "vec4") return 4;
    return 0;
  }
  static int Component(char field) {
    switch (field) {
      case 'x': case 'r': case 's': return 0;
      case 'y': case 'g': case 't': return 1;
      case 'z': case 'b': case 'p': return 2;
      case 'w': case 'a': case 'q': return 3;
      default: return -1;
    }
  }
  GlslValue Sum() {
    GlslValue left = Product();
    for (;;) {
      Skip();
      if (p >= end || (*p != '+' && *p != '-')) return left;
      const bool add = *p == '+';
      ++p;
      const GlslValue right = Product();
      if (left.count != right.count) { ok = false; return left; }
      for (int i = 0; i < left.count; ++i)
        left.value[i] = add ? left.value[i] + right.value[i] : left.value[i] - right.value[i];
    }
  }
  GlslValue Product() {
    GlslValue left = Unary();
    for (;;) {
      Skip();
      if (p >= end || (*p != '*' && *p != '/')) return left;
      const bool multiply = *p == '*';
      ++p;
      const GlslValue right = Unary();
      if (left.count != right.count || (!multiply && right.value[0] == 0.0f)) {
        ok = false;
        return left;
      }
      for (int i = 0; i < left.count; ++i)
        left.value[i] = multiply ? left.value[i] * right.value[i] : left.value[i] / right.value[i];
    }
  }
  GlslValue Unary() {
    Skip();
    if (p < end && *p == '-') {
      ++p;
      GlslValue value = Unary();
      for (int i = 0; i < value.count; ++i) value.value[i] = -value.value[i];
      return value;
    }
    if (p < end && *p == '+') { ++p; return Unary(); }
    return Primary();
  }
  // A component-wise swizzle of any value, e.g. "varyinTexCoordinate.xy".
  GlslValue Primary() {
    GlslValue out = Atom();
    Skip();
    if (p < end && *p == '.') {
      ++p;
      const std::string fields = Name();
      GlslValue swizzled;
      if (fields.empty() || fields.size() > 4) { ok = false; return out; }
      for (char field : fields) {
        const int index = Component(field);
        if (index < 0 || index >= out.count) { ok = false; return out; }
        swizzled.value[swizzled.count++] = out.value[index];
      }
      out = swizzled;
    }
    return out;
  }
  // A parenthesised sum, a number, a type constructor or an identifier.
  GlslValue Atom() {
    Skip();
    GlslValue out;
    if (p >= end) { ok = false; return out; }
    if (*p == '(') {
      ++p;
      out = Sum();
      if (!Take(')')) ok = false;
      return out;
    }
    if (std::isdigit(static_cast<unsigned char>(*p)) || *p == '.') {
      out.count = 1;
      if (!Number(&out.value[0])) ok = false;
      return out;
    }
    const std::string name = Name();
    if (name.empty()) { ok = false; return out; }
    Skip();
    if (p < end && *p == '(') {
      ++p;
      std::vector<float> parts;
      out.count = ConstructorSize(name);
      if (out.count == 0) { ok = false; return out; }
      for (;;) {
        const GlslValue arg = Sum();
        for (int i = 0; i < arg.count; ++i) parts.push_back(arg.value[i]);
        if (!Take(',')) break;
      }
      if (!Take(')') || parts.empty()) { ok = false; return out; }
      for (int i = 0; i < out.count; ++i)
        out.value[i] = parts[static_cast<size_t>(i) < parts.size() ? i : parts.size() - 1];
      return out;
    }
    const GlslValue* found = lookup ? lookup(context, name.c_str(), name.size()) : nullptr;
    if (!found) { ok = false; return out; }
    return *found;
  }
};
}  // namespace

bool GlslEvaluate(const std::string& expression, GlslLookup lookup, void* context,
                  GlslValue* out) {
  Parser parser{expression.data(), expression.data() + expression.size(), lookup, context};
  const GlslValue value = parser.Sum();
  parser.Skip();
  if (!parser.ok || parser.p != parser.end || value.count == 0) return false;
  *out = value;
  return true;
}
}  // namespace ruyijs::web_graphics
