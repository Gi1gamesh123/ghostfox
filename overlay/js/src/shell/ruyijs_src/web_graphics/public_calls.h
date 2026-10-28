#ifndef RUYIJS_WEB_GRAPHICS_PUBLIC_CALLS_H
#define RUYIJS_WEB_GRAPHICS_PUBLIC_CALLS_H
namespace ruyijs::web_graphics {
enum PublicOperation { kVoid, kNull, kZero, kMinusOne, kInvalidIndex, kFalse,
                      kEmptyString, kEmptyArray, kFramebufferComplete, kWaitComplete,
                      kAttributes, kEnable, kDisable, kIsEnabled, kBlendEquation,
                      kBlendEquationSeparate, kBlendFunc, kBlendFuncSeparate,
                      kBlendColor, kCullFace, kFrontFace, kLineWidth, kDepthFunc,
                      kDepthMask, kDepthRange, kClearDepth, kClearColor, kClearStencil,
                      kColorMask, kStencilFunc, kStencilFuncSeparate, kStencilMask,
                      kStencilMaskSeparate, kStencilOp, kStencilOpSeparate,
                      kPolygonOffset, kScissor, kViewport, kPixelStorei, kHint,
                      kSampleCoverage,
                      // draw_calls.cpp: the calls that carry real shader, buffer,
                      // attribute, uniform, draw and readback state.
                      kShaderSource, kCompileShader, kAttachShader, kLinkProgram,
                      kUseProgram, kBufferData, kGetAttribLocation,
                      kEnableVertexAttrib, kDisableVertexAttrib, kVertexAttribPointer,
                      kUniformFloat, kDrawArrays, kReadPixels, kGetProgramParameter,
                      kGetShaderParameter, kGetShaderSource, kGetAttachedShaders };
}
#endif
