#ifndef RUYIJS_DOMAIN_WEB_MEDIA_AUDIO_SAMPLES_H
#define RUYIJS_DOMAIN_WEB_MEDIA_AUDIO_SAMPLES_H
// WebCodecs AudioData sample formats and copies, ported from Firefox
// dom/media/AudioSampleFormat.h (ConvertAudioSample) and
// dom/media/webcodecs/AudioData.cpp (CopySamples). Pure rules; no engine use.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>
#include <type_traits>

namespace ruyijs::domain::web_media {
enum class SampleFormat { U8, S16, S32, F32, U8Planar, S16Planar, S32Planar, F32Planar };
inline constexpr std::string_view kSampleFormatNames[] = {
    "u8", "s16", "s32", "f32", "u8-planar", "s16-planar", "s32-planar", "f32-planar"};
inline bool ParseSampleFormat(std::string_view name, SampleFormat* out) {
  for (int i = 0; i < 8; ++i)
    if (kSampleFormatNames[i] == name) { *out = SampleFormat(i); return true; }
  return false;
}
inline bool Interleaved(SampleFormat f) { return f <= SampleFormat::F32; }
inline uint32_t BytesPerSample(SampleFormat f) {
  switch (f) {
    case SampleFormat::U8: case SampleFormat::U8Planar: return 1;
    case SampleFormat::S16: case SampleFormat::S16Planar: return 2;
    default: return 4;
  }
}
namespace samples {
template <typename T> constexpr float MaxF() { return float(std::numeric_limits<T>::max()); }
template <typename T> constexpr float LowF() { return float(std::numeric_limits<T>::lowest()); }
inline float Clip(float v) { return std::clamp(v, -1.0f, 1.0f); }
template <typename T> T FromFloat(float v) {
  if constexpr (std::is_same_v<float, T>) return v;
  else if constexpr (std::is_same_v<uint8_t, T>)
    return static_cast<T>(std::clamp((v + 1.0f) * 128.f, LowF<T>(), MaxF<T>()));
  else if constexpr (std::is_same_v<int16_t, T>)
    return static_cast<T>(std::clamp(v * -LowF<T>(), LowF<T>(), MaxF<T>()));
  else {
    if (v >= 0.) {
      if (v >= 1.0) return std::numeric_limits<int32_t>::max();
      return static_cast<int32_t>(v * double(std::numeric_limits<int32_t>::max()));
    }
    if (v <= -1.0) return std::numeric_limits<int32_t>::lowest();
    return static_cast<int32_t>(v * (-1.0 * std::numeric_limits<int32_t>::lowest()));
  }
}
template <typename T> T FromU8(uint8_t v) {
  if constexpr (std::is_same_v<uint8_t, T>) return v;
  else if constexpr (std::is_same_v<int16_t, T>) return T((int16_t(v) << 8) - (1 << 15));
  else if constexpr (std::is_same_v<int32_t, T>) return T(int32_t(uint32_t(v) << 24) - int32_t(1u << 31));
  else {
    const float biased = float(v) - 128;
    return v >= 128 ? Clip(biased / MaxF<int8_t>()) : Clip(biased / -LowF<int8_t>());
  }
}
template <typename T> T FromS16(int16_t v) {
  if constexpr (std::is_same_v<uint8_t, T>) return static_cast<uint8_t>(v >> 8) + 128;
  else if constexpr (std::is_same_v<int16_t, T>) return v;
  else if constexpr (std::is_same_v<int32_t, T>) return int32_t(uint32_t(int32_t(v)) << 16);
  else return v >= 0 ? Clip(float(v) / MaxF<int16_t>()) : Clip(float(v) / -LowF<int16_t>());
}
template <typename T> T FromS32(int32_t v) {
  if constexpr (std::is_same_v<uint8_t, T>) return static_cast<uint8_t>(v >> 24) + 128;
  else if constexpr (std::is_same_v<int16_t, T>) return T(v >> 16);
  else if constexpr (std::is_same_v<int32_t, T>) return v;
  else return v >= 0 ? Clip(float(v) / MaxF<int32_t>()) : Clip(float(v) / -LowF<int32_t>());
}
template <typename D, typename S> D Convert(S s) {
  if constexpr (std::is_same_v<S, D>) return s;
  else if constexpr (std::is_same_v<S, uint8_t>) return FromU8<D>(s);
  else if constexpr (std::is_same_v<S, int16_t>) return FromS16<D>(s);
  else if constexpr (std::is_same_v<S, int32_t>) return FromS32<D>(s);
  else return FromFloat<D>(s);
}
template <typename T> T Load(const uint8_t* p, size_t i) { T v; std::memcpy(&v, p + i * sizeof(T), sizeof(T)); return v; }
template <typename T> void Store(uint8_t* p, size_t i, T v) { std::memcpy(p + i * sizeof(T), &v, sizeof(T)); }
// Firefox CopySamples over typed element indices.
template <typename S, typename D>
void Copy(const uint8_t* src, uint8_t* dst, uint32_t channels, uint32_t frames, SampleFormat from,
          SampleFormat to, uint32_t count, uint32_t offset, uint32_t plane) {
  if (Interleaved(from) && Interleaved(to)) {
    for (size_t i = 0; i < size_t(count) * channels; ++i)
      Store<D>(dst, i, Convert<D>(Load<S>(src, size_t(offset) * channels + i)));
  } else if (Interleaved(from)) {
    size_t read = size_t(offset) * channels + plane;
    for (size_t i = 0; i < count; ++i, read += channels) Store<D>(dst, i, Convert<D>(Load<S>(src, read)));
  } else if (Interleaved(to)) {
    for (size_t c = 0; c < channels; ++c) {
      size_t write = c, read = c * frames + offset;
      for (size_t i = 0; i < count; ++i, ++read, write += channels)
        Store<D>(dst, write, Convert<D>(Load<S>(src, read)));
    }
  } else {
    const size_t base = size_t(plane) * frames + offset;
    for (size_t i = 0; i < count; ++i) Store<D>(dst, i, Convert<D>(Load<S>(src, base + i)));
  }
}
template <typename S>
void CopyFrom(const uint8_t* src, uint8_t* dst, uint32_t channels, uint32_t frames, SampleFormat from,
              SampleFormat to, uint32_t count, uint32_t offset, uint32_t plane) {
  switch (BytesPerSample(to) == 4 && (to == SampleFormat::F32 || to == SampleFormat::F32Planar) ? 3
          : BytesPerSample(to) == 4 ? 2 : BytesPerSample(to) == 2 ? 1 : 0) {
    case 0: return Copy<S, uint8_t>(src, dst, channels, frames, from, to, count, offset, plane);
    case 1: return Copy<S, int16_t>(src, dst, channels, frames, from, to, count, offset, plane);
    case 2: return Copy<S, int32_t>(src, dst, channels, frames, from, to, count, offset, plane);
    default: return Copy<S, float>(src, dst, channels, frames, from, to, count, offset, plane);
  }
}
}  // namespace samples
// Copies |count| frames from |offset| (one plane when |to| is planar) of the
// source resource into |dst|, converting each sample as Firefox does.
inline void CopyAudioSamples(const uint8_t* src, uint8_t* dst, uint32_t channels, uint32_t frames,
                             SampleFormat from, SampleFormat to, uint32_t count, uint32_t offset,
                             uint32_t plane) {
  using namespace samples;
  const bool isFloat = from == SampleFormat::F32 || from == SampleFormat::F32Planar;
  switch (isFloat ? 3 : BytesPerSample(from) == 4 ? 2 : BytesPerSample(from) == 2 ? 1 : 0) {
    case 0: return CopyFrom<uint8_t>(src, dst, channels, frames, from, to, count, offset, plane);
    case 1: return CopyFrom<int16_t>(src, dst, channels, frames, from, to, count, offset, plane);
    case 2: return CopyFrom<int32_t>(src, dst, channels, frames, from, to, count, offset, plane);
    default: return CopyFrom<float>(src, dst, channels, frames, from, to, count, offset, plane);
  }
}
}
#endif
