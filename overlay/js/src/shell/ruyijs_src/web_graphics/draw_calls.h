#ifndef RUYIJS_WEB_GRAPHICS_DRAW_CALLS_H
#define RUYIJS_WEB_GRAPHICS_DRAW_CALLS_H
#include <string>
#include <vector>

#include "js/CallArgs.h"
#include "js/RootingAPI.h"
#include "web_graphics/glsl.h"
#include "web_graphics/state.h"

namespace ruyijs::web_graphics::draw_call_detail {
// The ASCII text of a JS value: a shader source, a uniform or attribute name.
bool TextOf(JSContext*, JS::HandleValue, std::string*);
// The object in `value` when it is a live object of this kind owned by `context`.
JSObject* Owned(State&, Context&, JS::HandleValue, int32_t kind);
// The shader a program has attached for one stage: 0 vertex, 1 fragment.
JSObject* StageOf(JSObject* program, int32_t stage);
// The source a shader was given through shaderSource.
bool ShaderSource(JSContext*, JSObject* shader, std::string* out);
// The canvas size the drawing buffer takes: its width and height content
// attributes, which the canvas owner reflects onto the element.
void CanvasSize(JSContext*, Context&, int32_t* width, int32_t* height);
// The declarations of `kind` in the shader attached to the program's stage.
bool StageDeclarations(JSContext*, JSObject* program, int32_t stage, const char* kind,
                       std::vector<GlslDeclaration>* out);
}  // namespace ruyijs::web_graphics::draw_call_detail

namespace ruyijs::web_graphics {
// draw_program.cpp: shader/program state and the two parameter queries.
bool ProgramOperation(JSContext*, const JS::CallArgs&, State&, Context&, int op);
// draw_transfer.cpp: buffer data, attribute pointers, uniforms, draw and readback.
bool TransferOperation(JSContext*, const JS::CallArgs&, State&, Context&, int op);
}  // namespace ruyijs::web_graphics
#endif
