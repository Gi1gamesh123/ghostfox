#include "web_graphics/internal.h"
#include "web_graphics/public_calls.h"
#include "web_graphics/state_call_validation.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace ruyijs::web_graphics {
namespace {
bool Has(int32_t value, std::initializer_list<int32_t> values) {
  for (int32_t candidate : values) if (candidate == value) return true;
  return false;
}
bool EnumArg(JSContext* cx, const JS::CallArgs& args, unsigned i, int32_t* out) {
  return detail::ToEnum(cx, args.get(i), out);
}
bool NumberArg(JSContext* cx, const JS::CallArgs& args, unsigned i, double* out) {
  if (!JS::ToNumber(cx, args.get(i), out)) return false;
  *out = static_cast<float>(*out);
  return true;
}
void Put(Context& c, int32_t pname, ParameterKind kind, int count,
         const double* values) {
  SetParameterState(c, pname, kind, count, values);
}
void Put1(Context& c, int32_t pname, double value, ParameterKind kind = ParameterKind::Number) {
  Put(c, pname, kind, 1, &value);
}
bool Enabled(const Context& context, int32_t value) {
  ParameterStateValue state;
  if (GetParameterState(context, value, &state)) return state.values[0] != 0;
  // DITHER is enabled by default in WebGL's initial state.
  if (value == 0x0BD0) return true;
  return false;
}
void Enable(Context& context, int32_t value, bool enabled) {
  const double state = enabled ? 1.0 : 0.0;
  SetParameterState(context, value, ParameterKind::Boolean, 1, &state);
}
}

bool StateCall(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  const int op = WebKit::Slot(args) & 0xffff;
  const unsigned required = state_call_detail::RequiredArguments(op);
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  if (args.length() < required)
    return WebKit::Fail(cx, "WebGL method requires more arguments");
  Context& c = *detail::ContextOf(self);
  args.rval().setUndefined();
  int32_t e[4] = {};
  double v[4] = {};
  auto invalidEnum = [&]() { detail::SetError(c, kInvalidEnum); return true; };
  auto invalidValue = [&]() { detail::SetError(c, kInvalidValue); return true; };
  switch (op) {
    case kEnable: case kDisable: case kIsEnabled:
    {
      if (!EnumArg(cx,args,0,&e[0])) return false;
      bool validCapability = false;
      if (!state_call_detail::ValidateCapability(state, c, e[0], &validCapability)) return false;
      if (!validCapability) { detail::SetError(c,kInvalidEnum); args.rval().setBoolean(false); return true; }
      if (op == kIsEnabled) args.rval().setBoolean(Enabled(c,e[0]));
      else Enable(c,e[0],op == kEnable);
      return true;
    }
    case kBlendEquation: case kBlendEquationSeparate: {
      if (!EnumArg(cx,args,0,&e[0]) || (op == kBlendEquationSeparate && !EnumArg(cx,args,1,&e[1]))) return false;
      bool validRgb = false, validAlpha = true;
      if (!state_call_detail::ValidateBlendEquation(state, c, e[0], &validRgb)) return false;
      if (op == kBlendEquationSeparate &&
          !state_call_detail::ValidateBlendEquation(state, c, e[1], &validAlpha)) return false;
      if (!validRgb || !validAlpha) return invalidEnum();
      Put1(c,0x8009,e[0]); Put1(c,0x883D,op == kBlendEquation ? e[0] : e[1]); return true;
    }
    case kBlendFunc: case kBlendFuncSeparate: {
      unsigned n = op == kBlendFunc ? 2 : 4;
      for (unsigned i=0;i<n;++i) if (!EnumArg(cx,args,i,&e[i])) return false;
      for (unsigned i=0;i<n;++i) if (!state_call_detail::ValidBlendFactor(e[i])) return invalidEnum();
      const unsigned dstRgbIndex = 1;
      const unsigned dstAlphaIndex = n == 2 ? 1 : 3;
      if (c.version == 1 && (e[dstRgbIndex] == 0x0308 || e[dstAlphaIndex] == 0x0308))
        return invalidEnum();
      const int32_t srcRgb = e[0], dstRgb = e[1];
      const bool srcColor = srcRgb == 0x8001 || srcRgb == 0x8002;
      const bool srcAlpha = srcRgb == 0x8003 || srcRgb == 0x8004;
      const bool dstColor = dstRgb == 0x8001 || dstRgb == 0x8002;
      const bool dstAlpha = dstRgb == 0x8003 || dstRgb == 0x8004;
      if ((srcColor && dstAlpha) || (srcAlpha && dstColor)) {
        detail::SetError(c, kInvalidOperation); return true;
      }
      Put1(c,0x80C9,e[0]); Put1(c,0x80C8,e[1]);
      Put1(c,0x80CB,n==2?e[0]:e[2]); Put1(c,0x80CA,n==2?e[1]:e[3]); return true;
    }
    case kBlendColor: case kClearColor: case kColorMask: {
      for (unsigned i=0;i<4;++i) {
        if (op == kColorMask) v[i] = JS::ToBoolean(args.get(i));
        else if (!NumberArg(cx,args,i,&v[i])) return false;
      }
      if (op == kBlendColor && c.version == 1) {
        bool unclamped = false, enabled = false;
        if (!ExtensionEnabled(state, c, "WEBGL_color_buffer_float", &enabled)) return false;
        unclamped = enabled;
        if (!ExtensionEnabled(state, c, "EXT_color_buffer_half_float", &enabled)) return false;
        unclamped = unclamped || enabled;
        if (!unclamped) for (double& x : v) x = std::clamp(x,0.0,1.0);
      }
      Put(c,op == kBlendColor ? 0x8005 : op == kClearColor ? 0x0C22 : 0x0C23,
          op == kColorMask ? ParameterKind::BoolArray : ParameterKind::FloatArray,4,v);
      return true;
    }
    case kCullFace: case kFrontFace: case kDepthFunc: {
      if (!EnumArg(cx,args,0,&e[0])) return false;
      bool valid = op == kCullFace ? state_call_detail::ValidFace(e[0]) : op == kFrontFace ? Has(e[0],{0x0900,0x0901}) : state_call_detail::ValidCompare(e[0]);
      if (!valid) return invalidEnum();
      Put1(c,op == kCullFace ? 0x0B45 : op == kFrontFace ? 0x0B46 : 0x0B74,e[0]); return true;
    }
    case kLineWidth: case kClearDepth: case kPolygonOffset: {
      const unsigned n=op==kPolygonOffset?2:1;
      for(unsigned i=0;i<n;++i) if(!NumberArg(cx,args,i,&v[i])) return false;
      if (op == kLineWidth && !(v[0] > 0)) return invalidValue();
      if (op == kClearDepth) v[0] = std::clamp(v[0],0.0,1.0);
      if (op == kPolygonOffset) { Put1(c,0x8038,v[0]); Put1(c,0x2A00,v[1]); }
      else Put1(c,op==kLineWidth?0x0B21:0x0B73,v[0]);
      return true;
    }
    case kDepthMask: {
      v[0]=JS::ToBoolean(args.get(0)); Put1(c,0x0B72,v[0],ParameterKind::Boolean); return true;
    }
    case kDepthRange: case kSampleCoverage: {
      for(unsigned i=0;i<2;++i) if(!NumberArg(cx,args,i,&v[i])) return false;
      if(op==kDepthRange) {
        Put(c,0x0B70,ParameterKind::FloatArray,2,v);
        if (v[0] > v[1]) detail::SetError(c, kInvalidOperation);
      }
      else {
        // WebGL clamps SAMPLE_COVERAGE_VALUE to [0, 1] before exposing it
        // through getParameter; unlike depthRange, the clamped value is the
        // state value itself.
        v[0] = std::clamp(v[0], 0.0, 1.0);
        Put1(c,0x80AA,v[0]);
        Put1(c,0x80AB,JS::ToBoolean(args.get(1)),ParameterKind::Boolean);
      }
      return true;
    }
    case kClearStencil: case kStencilMask: {
      if(!JS::ToInt32(cx,args.get(0),&e[0])) return false;
      if(op==kClearStencil) Put1(c,0x0B91,e[0]);
      else { const double m=static_cast<uint32_t>(e[0]); Put1(c,0x0B98,m); Put1(c,0x8CA5,m); }
      return true;
    }
    case kStencilMaskSeparate: {
      if(!EnumArg(cx,args,0,&e[0]) || !JS::ToInt32(cx,args.get(1),&e[1])) return false;
      if(!state_call_detail::ValidFace(e[0])) return invalidEnum();
      const double m=static_cast<uint32_t>(e[1]);
      if(e[0]!=0x0405) Put1(c,0x0B98,m); if(e[0]!=0x0404) Put1(c,0x8CA5,m); return true;
    }
    case kStencilFunc: case kStencilFuncSeparate: {
      unsigned p=0; int32_t face=0;
      if(op==kStencilFuncSeparate) { if(!EnumArg(cx,args,0,&face)) return false; p=1; if(!state_call_detail::ValidFace(face)) return invalidEnum(); }
      if(!EnumArg(cx,args,p,&e[0]) || !JS::ToInt32(cx,args.get(p+1),&e[1]) || !detail::ToEnum(cx,args.get(p+2),&e[2])) return false;
      if(!state_call_detail::ValidCompare(e[0])) return invalidEnum();
      if(op==kStencilFunc || face!=0x0405) { Put1(c,0x0B92,e[0]); Put1(c,0x0B97,e[1]); Put1(c,0x0B93,static_cast<uint32_t>(e[2])); }
      if(op==kStencilFunc || face!=0x0404) { Put1(c,0x8800,e[0]); Put1(c,0x8CA3,e[1]); Put1(c,0x8CA4,static_cast<uint32_t>(e[2])); }
      return true;
    }
    case kStencilOp: case kStencilOpSeparate: {
      unsigned p=0; int32_t face=0;
      if(op==kStencilOpSeparate) { if(!EnumArg(cx,args,0,&face)) return false; p=1; if(!state_call_detail::ValidFace(face)) return invalidEnum(); }
      for(unsigned i=0;i<3;++i) if(!EnumArg(cx,args,p+i,&e[i])) return false;
      for(unsigned i=0;i<3;++i) if(!state_call_detail::ValidStencilOp(e[i])) return invalidEnum();
      const int32_t front[]={0x0B94,0x0B95,0x0B96}, back[]={0x8801,0x8802,0x8803};
      for(unsigned i=0;i<3;++i) { if(op==kStencilOp || face!=0x0405) Put1(c,front[i],e[i]); if(op==kStencilOp || face!=0x0404) Put1(c,back[i],e[i]); }
      return true;
    }
    case kScissor: case kViewport: {
      for(unsigned i=0;i<4;++i) if(!JS::ToInt32(cx,args.get(i),&e[i])) return false;
      if(e[2]<0 || e[3]<0) return invalidValue();
      for(unsigned i=0;i<4;++i) v[i]=e[i];
      Put(c,op==kScissor?0x0C10:0x0BA2,ParameterKind::IntArray,4,v); return true;
    }
    case kPixelStorei: {
      if(!EnumArg(cx,args,0,&e[0]) || !JS::ToInt32(cx,args.get(1),&e[1])) return false;
      if(e[1] < 0) return invalidValue();
      if(e[0]==0x0D05 || e[0]==0x0CF5) { if(!Has(e[1],{1,2,4,8})) return invalidValue(); }
      else if(e[0]==0x9240 || e[0]==0x9241) { Put1(c,e[0],e[1]!=0,ParameterKind::Boolean); return true; }
      else if(e[0]==0x9243) { if(!Has(e[1],{0,0x9244})) return invalidEnum(); }
      else if(e[0]==0x0D02 || e[0]==0x0D03 || e[0]==0x0D04 || e[0]==0x0CF2 || e[0]==0x0CF3 || e[0]==0x0CF4 || e[0]==0x806D || e[0]==0x806E) { if(c.version!=2) return invalidEnum(); }
      else return invalidEnum();
      Put1(c,e[0],e[1]); return true;
    }
    case kHint: {
      if(!EnumArg(cx,args,0,&e[0]) || !EnumArg(cx,args,1,&e[1])) return false;
      if(!Has(e[1],{0x1100,0x1101,0x1102})) return invalidEnum();
      if(e[0]==0x8192) Put1(c,0x8192,e[1]);
      else if(e[0]==0x8B8B) { bool enabled=false; if(c.version==1 && !ExtensionEnabled(*detail::Self(args),c,"OES_standard_derivatives",&enabled)) return false; if(c.version==1 && !enabled) return invalidEnum(); Put1(c,0x8B8B,e[1]); }
      else return invalidEnum();
      return true;
    }
    default: return true;
  }
}
}  // namespace ruyijs::web_graphics
