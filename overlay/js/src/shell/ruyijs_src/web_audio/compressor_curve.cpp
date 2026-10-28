#include "web_audio/compressor.h"

#include <algorithm>
#include <cmath>

#include "web_audio/dsp_math.h"

// The static compression curve of blink/DynamicsCompressorKernel.cpp: the
// exponential knee, the constant-ratio tail, the dB slope probe and the
// 15-step geometric search for the knee's k. Kept here so the kernel's main loop
// stays readable; the formulas are Gecko's, coefficient for coefficient.
namespace ruyijs::web_audio {
namespace {
// DynamicsCompressor::initializeParameters release zone values 0 -> 1.
constexpr float kReleaseZone1 = 0.09f;
constexpr float kReleaseZone2 = 0.16f;
constexpr float kReleaseZone3 = 0.42f;
constexpr float kReleaseZone4 = 0.98f;
}  // namespace

// The coefficients were derived for 4th order polynomial curve fitting where the
// y values match the evenly spaced x values 0, 1, 2, 3.
ReleaseCurve ReleaseCurveFor(float release_frames) {
  const float y1 = release_frames * kReleaseZone1;
  const float y2 = release_frames * kReleaseZone2;
  const float y3 = release_frames * kReleaseZone3;
  const float y4 = release_frames * kReleaseZone4;
  ReleaseCurve curve;
  curve.a = 0.9999999999999998f * y1 + 1.8432219684323923e-16f * y2 -
            1.9373394351676423e-16f * y3 + 8.824516011816245e-18f * y4;
  curve.b = -1.5788320352845888f * y1 + 2.3305837032074286f * y2 -
            0.9141194204840429f * y3 + 0.1623677525612032f * y4;
  curve.c = 0.5334142869106424f * y1 - 1.272736789213631f * y2 +
            0.9258856042207512f * y3 - 0.18656310191776226f * y4;
  curve.d = 0.08783463138207234f * y1 - 0.1694162967925622f * y2 +
            0.08588057951595272f * y3 - 0.00429891410546283f * y4;
  curve.e = -0.042416883008123074f * y1 + 0.1115693827987602f * y2 -
            0.09764676325265872f * y3 + 0.028494263462021576f * y4;
  return curve;
}

// Exponential curve for the knee, 1st derivative matched at the linear
// threshold and asymptotically approaching linear_threshold + 1/k.
float CompressorKernel::KneeCurve(float x, float k) {
  if (x < linear_threshold_) return x;
  return linear_threshold_ +
         (1.0f - dsp::Exp(-k * (x - linear_threshold_))) / k;
}

// Full compression curve with constant ratio after the knee.
float CompressorKernel::Saturate(float x, float k) {
  if (x < knee_threshold_) return KneeCurve(x, k);
  const float x_db = dsp::ToDecibels(x, -1000.0f);
  const float y_db = yknee_threshold_db_ + slope_ * (x_db - knee_threshold_db_);
  return dsp::ToLinear(y_db);
}

// Approximate 1st derivative with input and output expressed in dB.
float CompressorKernel::SlopeAt(float x, float k) {
  if (x < linear_threshold_) return 1.0f;
  const float x2 = x * 1.001f;
  const float x_db = dsp::ToDecibels(x, -1000.0f);
  const float x2_db = dsp::ToDecibels(x2, -1000.0f);
  const float y_db = dsp::ToDecibels(KneeCurve(x, k), -1000.0f);
  const float y2_db = dsp::ToDecibels(KneeCurve(x2, k), -1000.0f);
  return (y2_db - y_db) / (x2_db - x_db);
}

float CompressorKernel::KAtSlope(float desired_slope) {
  const float x_db = db_threshold_ + db_knee_;
  const float x = dsp::ToLinear(x_db);
  float min_k = 0.1f;
  float max_k = 10000.0f;
  float k = 5.0f;
  for (int i = 0; i < 15; ++i) {
    const float slope = SlopeAt(x, k);
    if (slope < desired_slope) max_k = k;
    else min_k = k;
    k = std::sqrt(min_k * max_k);
  }
  return k;
}

float CompressorKernel::UpdateStaticCurveParameters(float db_threshold,
                                                    float db_knee, float ratio) {
  if (db_threshold != db_threshold_ || db_knee != db_knee_ || ratio != ratio_) {
    db_threshold_ = db_threshold;
    linear_threshold_ = dsp::ToLinear(db_threshold);
    db_knee_ = db_knee;
    ratio_ = ratio;
    slope_ = 1.0f / ratio_;
    const float k = KAtSlope(1.0f / ratio_);
    knee_threshold_db_ = db_threshold + db_knee;
    knee_threshold_ = dsp::ToLinear(knee_threshold_db_);
    yknee_threshold_db_ = dsp::ToDecibels(KneeCurve(knee_threshold_, k), -1000.0f);
    k_ = k;
  }
  return k_;
}

}  // namespace ruyijs::web_audio
