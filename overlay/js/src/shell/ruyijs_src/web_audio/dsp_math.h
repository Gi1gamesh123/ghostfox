#ifndef RUYIJS_WEB_AUDIO_DSP_MATH_H
#define RUYIJS_WEB_AUDIO_DSP_MATH_H
#include <algorithm>

// Gecko evaluates the Web Audio DSP in single precision with the fdlibm
// routines vendored in modules/fdlibm (dom/media/webaudio calls fdlibm_expf,
// fdlibm_log10f, fdlibm_powf, fdlibm_sinf, fdlibm_asinf). The platform libm
// differs from them by an ulp often enough to move the compressor's static
// curve, so the offline renderer calls this port instead of <cmath>.
namespace ruyijs::web_audio::dsp {

float Exp(float x);
float Log(float x);
float Log10(float x);
float Pow(float x, float y);
float Sin(float x);
float Cos(float x);
float Asin(float x);

// WebAudioUtils::ConvertLinearToDecibels / ConvertDecibelsToLinear.
inline float ToDecibels(float linear, float minusInfinityDb) {
  return linear > 0.0f ? 20.0f * Log10(linear) : minusInfinityDb;
}
inline float ToLinear(float decibels) { return Pow(10.0f, 0.05f * decibels); }

}  // namespace ruyijs::web_audio::dsp
#endif
